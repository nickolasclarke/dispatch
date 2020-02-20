import json
import itertools
import subprocess
import shutil
import glob

import requests
import partridge as ptg

from os.path import basename

BASE_URL = 'https://api.transitfeeds.com/v1/'
KEY      = 'd8243bad-de5a-47f7-8103-6d3c064d08da'

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

def get_feeds(page):
    '''returns a 
    '''
    r = requests.get(BASE_URL+'getFeeds', params = {'key':KEY,
                                         'type':'gtfs',
                                         'location':'undefined',
                                         'page':str(page),
                                         'descendants':'1',
                                         'limit':'100'
                                        })
    return r

def get_feed(fid):
    '''
    Download the GTFS feed blob from the transitfeeds API.
    fid: the transitfeeds `id` of the feed desired 
    '''
    clean_fid = ''.join(e for e in fid if e.isalnum())
    filename = f'{clean_fid}_gtfs.zip'
    r = requests.get(BASE_URL+'getLatestFeedVersion', params ={'key':KEY,
                                                               'feed':fid})
    with open(filename, 'wb') as f:
        f.write(r.content)
    return filename

def validate_feed(path):
    try:
        feed = ptg.load_feed(path)
    except Exception as err:
        new_path = shutil.move(path, '../data/feeds/unloadable/')
        print('Cannot load input as a GTFS feed', err)
        return new_path
    try:
        assert feed.routes.route_type.isin(itertools.chain(*route_types)).any()
        new_path = shutil.move(path, '../data/feeds/success/')
        return new_path
    except Exception as err:
        new_path = shutil.move(path, '../data/feeds/no_bus/')
        print('no Bus/Coach route_types found', err)
        return new_path
        
    # if feed.routes.route_type.isin(itertools.chain(*route_types)).any():
    #     new_path = shutil.move(path, '../data/feeds/success/')
    #     return new_path
    # else:
    #     new_path = shutil.move(path, '../data/feeds/no_bus/')
    #     print('no Bus/Coach route_types found')
    #     return new_path
    #     # move to another folder

#get the first page of results to cache the number of pages of results
#TODO store this result as well
response = get_feeds(1)
pages = response.json()['results']['numPages']
feeds = []
#pull and store all results
for i in range(1, pages + 1):
    feeds.append(get_feeds(i).json()['results']['feeds'])
# flatten the feed list
feeds = [feed for page in feeds for feed in page]
# download the feeds
[get_feed(feed['id']) for feed in feeds]

#validate and move feeds respectively
#TODO, use sys.argv input for relative path
[validate_feed(row) for row in glob.glob('../data/feeds/*.zip')]

#validate for block_id, and then simply attempt to run the feed.
def validate_feed(path):
    try:
        feed = ptg.load_feed(path)
    except Exception as err:
        #unload_path = shutil.move(path, '../data/feeds/unloadable/')
        print(f'Cannot load GTFS feed: {path}', err)
        #return unload_path
    # try:
    #     assert feed.routes.route_type.isin(itertools.chain(*route_types)).any()
    #     succ_path = shutil.move(path, '../data/feeds/success/')
    # except Exception as err:
    #     nobus_path = shutil.move(path, '../data/feeds/no_bus/')
    #     print(f'no Bus/Coach route_types found in feed: {path}', err)
    #     return nobus_path
    try: 
        assert feed.trips.columns.isin(['block_id']).any()
        # block_path = shutil.move(succ_path, 'includes_block/') #TODO how to use relative paths?
        block_path = shutil.move(path, '../data/feeds/success/includes_block/') #TODO temp, remove for above line.
    except AttributeError as err:
        print(f'no block_id found in the feed: {path}', err) #TODO broken now that I use .isin().any() for checking block_id
    except Exception as err:
         print(f'Unexpected error in validating feed: {path}', err)

def parse_feed(path):
    feed_name = basename(path).split('.')[0]
    try:
        a = subprocess.check_output(['python',
                                     'parse_gtfs.py',
                                     path,
                                     feed_name,
                                    ], stderr=subprocess.STDOUT)
        return(str(f''))
    except subprocess.CalledProcessError as cpe:
        print(cpe.output)
        return(cpe.output)

parsed_results = [parse_feed(path) for path in glob.glob('../data/feeds/success/includes_block/*.zip')]
