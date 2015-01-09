import requests
import datetime

url = "http://localhost:12084/leaderboard/"
payload = { 'initials': 'AAA', 'score': '1000666', 'date' : datetime.datetime.now().isoformat(), 'altar' : 'OG' }
print payload
files = { 'file' : open('test.gif', 'rb') }

while (True):
  r = requests.post(url, data = payload, files = files)
  print r.text

# r = requests.post(url, data = payload, files = files)
# print r.text