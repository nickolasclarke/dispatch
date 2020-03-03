#!/usr/bin/env python3
import csv
import json
import itertools
import subprocess
import shutil
import glob
import sqlite3
import sys
import multiprocessing
import os
import time

import requests
import partridge as ptg
import shelve

from multiprocessing import Pool
from os.path import basename

BASE_URL = 'https://api.transitfeeds.com/v1/'
KEY      = 'd8243bad-de5a-47f7-8103-6d3c064d08da'

#GTFS uses numeric identifiers to indicate what kind of vehicles serve a route.
#The relevant bus-like identifiers for us are below.
route_types = [
    [3], # Standard Route Type code

    [700, 701, #Extended Bus codes
    702, 703, 
    704, 705, 
    706, 707, 
    708, 709, 
    710, 711, 
    712, 713, 
    714, 715, 716],

    [200, 201, #Extended Coach codes
    202, 203, 
    204, 205, 
    206, 207, 
    208, 209],

    [800] #Extended Trollybus codes
]



class FeedFetcher:
    """Handles communication with TransitFeeds.com"""
    def __init__(self, base_url, key, feed_fn_template, workers=10):
        """Args:
        base_url - Base URL for data site
        key      - API key for the site
        feed_fn_template - Name such as 'data_dir/gtfs_{feed}.zip'
        workers  - Number of workers to use for downloading
        """
        self.base_url = base_url
        self.key      = key
        self.feed_fn_template = feed_fn_template
        self.workers = workers

    def get_feed_page(self, page):
        '''
        Fetches a list of transit feeds and associated meta data, returning them 
        as a JSON object.

        Args:
            page - What page to get from the results
        '''
        r = requests.get(
            self.base_url+'getFeeds', 
            params = {
                'key':         self.key,
                'type':        'gtfs',
                'location':    'undefined',
                'page':        str(page),
                'descendants': '1',
                'limit':       '100'
            }
        )
        return r.json()

    def get_num_pages(self):
        '''Determine how many pages of transit feeds there are'''
        #Get the first page of results to determine total number of pages
        return self.get_feed_page(1)['results']['numPages']

    @classmethod
    def clean_fid(cls, fid):
        '''Strip invalid characters from a feed id to turn it into a form 
           appropriate for filenames.
        
        Args:
            fid - The feed id to convert
        '''
        return ''.join(c for c in fid if c.isalnum())

    @classmethod
    def url_to_file_worker(cls, out_queue, in_queue):
        """Worker that takes work items from `out_queue`, tries to download the
        urls therein and saves them to disk, reports back on success/failure via
        `in_queue`"""
        while True:
            item = out_queue.get(block=True)                  # Retrieve job
            try:
                print(f"Fetching {item['id']}...")
                r = requests.get(item["url"], item["params"]) # Request data
                assert r.status_code == requests.codes.ok     # Make sure it's okay
                with open(item["outfn"], 'wb') as f:          # Open output file
                    f.write(r.content)                        # Write data
                in_queue.put(item["id"])                      # Signal success
            except:
                in_queue.put("!"+item["id"])                  # Signal failure

    def get_feeds_data(self, feed_list, updated):
        """Get data associated with each of the feed ids in `feed_list` and save it to disk
        
        Args:
            feed_list - List of feed ids to get
            updated   - Function to call when a feed is successfully updated
        """
        out_queue = multiprocessing.Queue() # To send data to workers
        in_queue  = multiprocessing.Queue() # To get data back from workers
        # Pool of workers for downloading data
        pool  = multiprocessing.Pool(self.workers, self.url_to_file_worker, (out_queue,in_queue))
        # Url to get feed data from
        url   = self.base_url+'getLatestFeedVersion'
        # Load work onto the queue
        for fid in feed_list:
            outfn = self.feed_fn_template.format(feed=self.clean_fid(fid))
            out_queue.put({"id":fid, "url":url, "params":{'key':self.key, 'feed':fid}, "outfn":outfn})
        while len(feed_list)>0:                        # If there's work left to do
            saved = in_queue.get(block=True)           # Wait for a message from a worker
            if saved[0]=="!": #Failed to get data      # If that message indicates failiure
                print(f"Failed to fetch {saved[1:]}!") # Notify the user
                feed_list.remove(saved[1:])            # And give up for now by removing from the fetch list
            else:                                      # Otherwise
                print(f"Fetched {saved}!")             # Notify the user
                feed_list.remove(saved)                # Remove from the fetch list
                updated(saved)                         # Use callback to indicate success
        pool.terminate()                               # Terminate the worker processes

    def get_feed_metadata(self):
        """Gets a list of all of the feeds' metadata"""

        # Number of pages of feeds in the feed listing
        pages = self.get_num_pages()

        # Acquire all of the feeds by looping over the pages
        feeds = []
        for i in range(1, pages+1):
            feeds.append(self.get_feed_page(i)['results']['feeds'])

        # Flatten the feed list
        feeds = [feed for page in feeds for feed in page]

        return feeds



class FeedManager:
    """Persistently manages information about feeds and whether we have their data."""
    def __init__(self, db_filename):
        self.db = shelve.open(db_filename, writeback=True)

    def __del__(self):
        self.db.close()
        
    def update(self, feed_data):
        """Given feed data, adds this data to the database and determines if any
        feeds need to be updated.
        """
        for f in feed_data:
            if 'latest' not in f:
                print(f"No latest data for '{f['t']}'! Skipping.")
                continue
            fid = f['id']
            name = f['t']
            url = None
            if 'd' in f['u']:
                url = f['u']['d']
            elif 'i' in f['u']:
                url = f['u']['i']
            latest = f['latest']['ts']

            if fid not in feed_data:              #We don't know about the feed
                self.db[fid] = {"name":name, "url":url, "latest":latest, "needs_update": True}
            elif feed_data[fid]['latest']<latest: #We do, but our data is old
                self.db[fid]["latest"] = latest
                self.db[fid]["needs_update"] = True
        self.db.sync()

    def feeds_to_update(self):
        """Get a list of feed ids for feeds that need updating"""
        return [fid for fid in self.db if self.db[fid]['needs_update']]
    
    def updated(self, fid):
        """Indicates that data has been acquired for the specified feed"""
        self.db[fid]["needs_update"] = False
        self.db.sync()

    def invalidate_all(self):
        """Indicates that all of the feeds need to have their data acquired"""
        for f in self.db:
            self.db[f]["needs_update"] = True
        self.db.sync()



if len(sys.argv)!=3:
    print(f"Syntax {sys.argv[0]} <Feeds DB> <Data Template Name>")
    print("Data Template Name should be like: 'data_dir/gtfs_{feed}.zip'")
    sys.exit(-1)

feeds_db_filename  = sys.argv[1]
data_template_name = sys.argv[2]

ff = FeedFetcher(
    base_url='https://api.transitfeeds.com/v1/',
    key='d8243bad-de5a-47f7-8103-6d3c064d08da',
    feed_fn_template=data_template_name
)

# Get data about all of the feeds
feed_data = ff.get_feed_metadata()

fm = FeedManager(db_filename=feeds_db_filename)

# Download feed metadata: determines if there are any new feeds or any feeds
# which have new data
fm.update(feed_data)

# Get updated data for those feeds which need it
ff.get_feeds_data(fm.feeds_to_update(), fm.updated)



#TODO: Validate feeds

"""


def validate_feed(path):
    try:
        feed = ptg.load_feed(path)
    except Exception as err:
        unload_path = shutil.move(path, '../data/feeds/unloadable/')
        print(f'Cannot load GTFS feed: {path}', err)
        return unload_path
    try:
        assert feed.routes.route_type.isin(itertools.chain(*route_types)).any()
        succ_path = shutil.move(path, '../data/feeds/success/')
    except Exception as err:
        nobus_path = shutil.move(path, '../data/feeds/no_bus/')
        print(f'no Bus/Coach route_types found in feed: {path}', err)
        return nobus_path
    try: 
        assert feed.trips.columns.isin(['block_id']).any()
        block_path = shutil.move(succ_path, '../data/feeds/success/includes_block/') #TODO how to use relative paths?
    except AttributeError as err:
        print(f'no block_id found in the feed: {path}', err) #TODO this logic is never used now that I use .isin().any() for checking block_id. How to re-introduce?
    except Exception as err:
         print(f'Unexpected error in validating feed: {path}', err)


def parse_feed(path):
    feed_name = basename(path).split('.')[0]
    print(f'parsing {feed_name}')
    try:
        a = subprocess.check_output(['python',
                                     'parse_gtfs.py',
                                     path,
                                     feed_name,
                                    ], stderr=subprocess.STDOUT)
        shutil.move(path, '../data/feeds/success/includes_block/parsed/') #TODO this does not appear to be working
        return({'name':str(f'{feed_name}'),'error': 'NaN'})
    except subprocess.CalledProcessError as cpe:
        print(cpe.output)
        return({'name':str(f'{feed_name}'),'error':cpe.output})
    except Exception as err:
        print('Unexpected error:', err)

if len(sys.argv)!=3:
    print(f"Syntax: {sys.argv[0]} <Feeds Database> <Data Directory>")
    sys.exit(-1)


[validate_feed(row) for row in glob.glob('../data/feeds/*.zip')] #TODO parallel this
parsed_results = parse_p.map(parse_feed, glob.glob('../data/feeds/*.zip'))

# parsed_results = [parse_feed(path) for path in glob.glob( #TODO parallel this
#                  '../data/feeds/success/includes_block/*.zip'
#                  )]

with open('validation_results.csv', 'w') as csv_file:
    dict_writer = csv.DictWriter(csv_file, parsed_results[0].keys())
    dict_writer.writeheader()
    dict_writer.writerows(parsed_results)

"""