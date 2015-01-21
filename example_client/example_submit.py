import requests
import datetime
import time

url = "http://localhost:12084/leaderboard/"
payload = { 'initials': 'AAA', 'score': '1000666', 'date' : datetime.datetime.now().isoformat(), 'altar' : 'OG' }
print payload

while (True):
  payload['date'] = datetime.datetime.now().isoformat()
  files = { 'file' : open('test.gif', 'rb') }
  r = requests.post(url, data = payload, files = files)
  #time.sleep(0.2)
  print r.text

# r = requests.post(url, data = payload, files = files)
# print r.text