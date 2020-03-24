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

import parse_gtfs


def clean_fid(fid):
    '''Strip invalid characters from a feed id to turn it into a form
       appropriate for filenames.

    Args:
        fid - The feed id to convert
    '''
    return ''.join(c for c in fid if c.isalnum())



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
            outfn = self.feed_fn_template.format(feed=clean_fid(fid))
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
    def __init__(self, db_filename, feed_fn_template):
        self.db = shelve.open(db_filename, writeback=True)
        self.feed_fn_template = feed_fn_template

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

    def needs_update_all(self):
        """Indicates that all of the feeds need to have their data acquired"""
        for f in sorted(self.db):
            self.db[f]["needs_update"] = True
        self.db.sync()

    def validate_feed(self, fid, parsed_prefix):
        print(f"Validating '{fid}'... ", end='')
        filename = self.feed_fn_template.format(feed=clean_fid(fid))
        try:
            if not parse_gtfs.DoesFeedLoad(filename):
                self.db[fid]["validation_status"] = "cannot_load"
                print("cannot load")
            elif not parse_gtfs.HasBusRoutes(filename):
                self.db[fid]["validation_status"] = "no_buses"
                print("no buses")
            elif not parse_gtfs.HasBlockIDs(filename):
                self.db[fid]["validation_status"] = "no_blocks"
                print("no blocks")
            else:
                parse_gtfs.ParseFile(filename, parsed_prefix.format(feed=clean_fid(fid)))
                self.db[fid]["validation_status"] = "good"
                print("good")
        except Exception as err:
            print(f"ERROR on '{fid}': {err}")
            self.db[fid]["validation_status"] = "error: " + str(err)
        self.db.sync()

    def validate_feeds(self, parsed_prefix):
        for fid in sorted(self.db):
            if self.db[fid].get("validation_status", "unchecked")=="unchecked":
                self.validate_feed(fid, parsed_prefix)

    def invalidate_feeds(self):
        for fid in sorted(self.db):
            self.db[fid]["validation_status"] = "unchecked"
        self.db.sync()

    def print_validation(self):
        for fid in sorted(self.db):
            print(f"{fid:<50}: ", self.db[fid].get('validation_status', "unchecked"))

    def get_extents(self):
        for fid in sorted(self.db):
            if self.db[fid].get('extents', None) is not None:
                continue
            filename = self.feed_fn_template.format(feed=clean_fid(fid))
            extents  = parse_gtfs.GetExtents(filename)
            self.db[fid]['extents'] = extents
            self.db.sync()
            print(f"{fid} {extents}")



def AcquireFeeds(feeds_db_filename, data_template_name):
    ff = FeedFetcher(
        base_url='https://api.transitfeeds.com/v1/',
        key='d8243bad-de5a-47f7-8103-6d3c064d08da',
        feed_fn_template=data_template_name
    )

    # Get data about all of the feeds
    feed_data = ff.get_feed_metadata()

    fm = FeedManager(db_filename=feeds_db_filename, feed_fn_template=data_template_name)

    # Download feed metadata: determines if there are any new feeds or any feeds
    # which have new data
    fm.update(feed_data)

    # Get updated data for those feeds which need it
    ff.get_feeds_data(fm.feeds_to_update(), fm.updated)



def ValidateFeeds(feeds_db_filename, data_template_name, parsed_prefix):
    fm = FeedManager(db_filename=feeds_db_filename, feed_fn_template=data_template_name)
    fm.validate_feeds(parsed_prefix)



def InvalidateFeeds(feeds_db_filename, data_template_name):
    fm = FeedManager(db_filename=feeds_db_filename, feed_fn_template=data_template_name)
    fm.invalidate_feeds()



def ShowValidation(feeds_db_filename, data_template_name):
    fm = FeedManager(db_filename=feeds_db_filename, feed_fn_template=data_template_name)
    fm.print_validation()



def GetExtents(feeds_db_filename, data_template_name):
    fm = FeedManager(db_filename=feeds_db_filename, feed_fn_template=data_template_name)
    fm.get_extents()



def help():
    print(f"Syntax {sys.argv[0]} <Command> <Feeds DB> <Data Template Name> [Parsed Prefix]")
    print("Command can be 'acquire' or 'validate' or 'show_validation'")
    print("Data Template Name should be like: 'data_dir/gtfs_{feed}.zip'")
    print("Parsed Prefix should be like: 'parsed_dir/{feed}'")
    print("[Parsed Prefix] is only needed for 'validate'")
    sys.exit(-1)

command = sys.argv[1] if len(sys.argv)>=2 else "help"
if command=="help":
    help()
elif command=="acquire":
    feeds_db_filename  = sys.argv[2]
    data_template_name = sys.argv[3]
    AcquireFeeds(feeds_db_filename, data_template_name)
elif command=="validate":
    feeds_db_filename  = sys.argv[2]
    data_template_name = sys.argv[3]
    parsed_prefix      = sys.argv[4]
    ValidateFeeds(feeds_db_filename, data_template_name, parsed_prefix)
elif command=="show_validation":
    feeds_db_filename  = sys.argv[2]
    data_template_name = sys.argv[3]
    ShowValidation(feeds_db_filename, data_template_name)
elif command=="invalidate":
    feeds_db_filename  = sys.argv[2]
    data_template_name = sys.argv[3]
    InvalidateFeeds(feeds_db_filename, data_template_name)
elif command=="extents":
    feeds_db_filename  = sys.argv[2]
    data_template_name = sys.argv[3]
    GetExtents(feeds_db_filename, data_template_name)
else:
    print("Unrecognized command!")
    sys.exit(-1)
