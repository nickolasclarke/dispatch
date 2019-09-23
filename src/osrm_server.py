#!/usr/bin/env python3
import json
import requests

def GetRoute(lat1,lon1,lat2,lon2):
  """Get distance and time from (lat1,lon1) to (lat2,lon2).
  Example: ret = GetRoute(lat1=45.115789, lon1=-93.441584, lat2=44.891488, lon2=-93.077822)
  """
  routing_template = "http://127.0.0.1:5000/route/v1/driving/{lon1},{lat1};{lon2},{lat2}?steps=true"
  routing = routing_template.format(
    lat1 = lat1,
    lon1 = lon1,
    lat2 = lat2,
    lon2 = lon2
  )
  response = requests.get(routing)
  if response.status_code!=200:
    raise Exception("Failed to get route from server!")
  response = json.loads(response.content)
  if response['code']!='Ok':
    raise Exception('Server had a problem generating route!')
  route = response['routes'][0]
  return {"duration": route['duration'], "distance": route['distance']}
