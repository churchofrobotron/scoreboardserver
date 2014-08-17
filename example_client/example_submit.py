import requests
import datetime

url = "http://localhost:12084/leaderboard/"
payload = { 'initials': 'BTR', 'score': '1000666', 'date' : datetime.datetime.now().isoformat(), 'altar' : 'OG' }
print payload
files = { 'file' : open('test.gif', 'rb') }

r = requests.post(url, data = payload, files = files)
print r.text