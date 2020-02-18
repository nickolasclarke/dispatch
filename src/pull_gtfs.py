import json
import itertools
import shutil
import glob

import requests
import partridge as ptg

from folderstats import folderstats as fs

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


# feed_df = fs('../data/feeds/')
# feed_df = feed_df.drop_duplicates()
# feed_df = feed_df[feed_df['extension'] =='zip']
# feed_df = feed_df.drop_duplicates()
# feed_df = feed_df.reset_index(drop=True)


#validate and move feeds respectively
#TODO, store paths names
[validate_feed(row) for row in glob.glob('../data/feeds/*.zip')]

#validate for block_id, and then simply attempt to run the feed.

