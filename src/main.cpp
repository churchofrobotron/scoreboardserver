//
//  main.cpp
//  scoreboardserver
//
//  Created by Brian Richardson on 7/9/14.
//  Copyright (c) 2014 churchofrobotron. All rights reserved.
//

// TODO: Client side shiz

#include <string>
#include <deque>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <chrono>
#include <functional>
#include <algorithm>

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#include <ctime>
#include <cstring>
#include "dirent.h"

#include "mongoose.h"

using namespace std;
using namespace std::chrono;

const int SEMAPHORE_COUNT = 16;
//string document_root = "/Users/bzztbomb/projects/churchOfRobotron/scoreboardserver/www";
string document_root = "/home/pi/cor/scoreboardserver/www";

int mg_get_enc_var(const char* data, size_t data_len, const char* name, char* dst, size_t dst_len) {
  char search_buf[64];
  const char *p, *e, *s;
  int len;

  if (dst == NULL || dst_len == 0) {
    len = -2;
  } else if (data == NULL || name == NULL || data_len == 0) {
    len = -1;
    dst[0] = '\0';
  } else {
    sprintf(search_buf, "Content-Disposition: form-data; name=\"%s\"", name);
    e = data + data_len;
    len = -1;
    dst[0] = '\0';

    s = strstr(data, search_buf);
    if (s != NULL)
    {
      s += strlen(search_buf);
      while ((s < e) && ((*s == '\r') || (*s == '\n')))
        s++;
      for (len = 0; (s < e) && (len < dst_len - 1) && (*s != '\r') && (*s != '\n'); len++, s++)
        dst[len] = *s;
      dst[len] = '\0';
    }
  }
  return len;
}

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
  return elems;
}


std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> elems;
  split(s, delim, elems);
  return elems;
}

struct PlayerScore
{
  string mInitials;
  int mScore;
  string mDateTime; // iso datetime
  string mAltar; // alter a1bbrev
  string mFilename; //
  
  string toJSON() const;
  string toFilename() const;
  
  bool parseFilename(std::string filename);
};

bool PlayerScore::parseFilename(std::string filename)
{
  auto extPos = filename.rfind(".gif");
  if (extPos == string::npos)
    return false;
  mFilename = filename;
  filename.erase(extPos);
  vector<string> items = split(filename, '_');
  if (items.size() < 2)
    return false;
  mInitials = items[0];
  mScore = stoi(items[1]);
  mDateTime = items.size() > 2 ? items[2] : "2012-07-23T21:01:53.307380"; // toorcampish
  mAltar = items.size() > 3 ? items[3] : "TC"; // toorcamp
  return true;
}

string PlayerScore::toJSON() const
{
  string ret = "{ ";
  ret += "\"initials\": \"" + mInitials + "\", ";
  ret += "\"score\": " + to_string(mScore) + ", ";
  ret += "\"date\": \"" + mDateTime + "\", ";
  ret += "\"altar\": \"" + mAltar + "\", ";
  ret += "\"filename\": \"../scores/" + toFilename() + "\"";
  ret += " }";
  return ret;
}

string PlayerScore::toFilename() const
{
  if (mFilename.size())
    return mFilename;
  return mInitials + "_" + to_string(mScore) + "_" + mDateTime + "_" + mAltar + ".gif";
}

typedef std::deque<PlayerScore> PlayerScores;

PlayerScores getScores()
{
  PlayerScores ret;

  string scores_dir = document_root + "/scores/";

  DIR *dirp = opendir(scores_dir.c_str());
  struct dirent * dp;
  while ((dp = readdir(dirp)) != NULL) {
    try {
      PlayerScore score;
      string filename = dp->d_name;
      if (filename == "." || filename == "..")
        continue;
      if (!score.parseFilename(filename))
        continue;
      ret.push_back(score);
    } catch (...) {
      //
    }
  }
  closedir(dirp);
  return ret;
}

PlayerScores getTop(PlayerScores scores, int howmany, std::function<bool(const PlayerScore& a, const PlayerScore& b)> compare)
{
  sort(scores.begin(), scores.end(), compare);
  PlayerScores ret;
  for (int i = 0; i < min(howmany, (int) scores.size()); i++)
    ret.push_back(scores[i]);
  return ret;
}

PlayerScores getSubset(PlayerScores scores, std::function<bool(const PlayerScore& a)> pass_filter)
{
  PlayerScores ret;
  for (auto i : scores)
    if (pass_filter(i))
      ret.push_back(i);
  return ret;
}

string scoresToJSON(const PlayerScores& scores)
{
  string ret = "[";
  for (int i = 0; i < scores.size(); i++)
  {
    PlayerScore s = scores[i];
    ret += i ? ",\n" + s.toJSON() : s.toJSON();
  }
  ret += "]";
  return ret;
}

string scoreSummary(const PlayerScores& allTime, const PlayerScores& lastDay, const PlayerScores& mostRecent)
{
  char buffer[128];
  system_clock::time_point now = system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  strftime(buffer, sizeof(buffer), "%FT%T", std::localtime(&t));
  
  string ret = "{";

  ret += "\"timestamp\" : \"" + string(buffer) + "\",\n";
  
  typedef std::pair<std::string, std::string> StringPair;
  StringPair scores[] = {
    StringPair("All Time High Scores", scoresToJSON(allTime)),
    StringPair("High Scores in the Last 24 hours", scoresToJSON(lastDay)),
    StringPair("Most Recent Players", scoresToJSON(mostRecent))
  };
  
  ret += "\"score_types\" : [\n";
  bool first = true;
  for (auto p : scores)
  {
    ret += (first ? "{" : ",{");
    first = false;
    ret += "\"name\": \"" + p.first + "\",";
    ret += "\"scores\": " + p.second;
    ret += "}\n";
  }
  ret += "]\n";
  ret += "}";
  return ret;
}

struct CurrentScores
{
  PlayerScores mAllScores;
  PlayerScores mAllTime;
  PlayerScores mLastDay;
  PlayerScores mMostRecent;

  CurrentScores()
  {

  }
};

CurrentScores currentScores;
pthread_mutex_t newScoreMutex;

string responseA, responseB;
string* currentResponse = &responseA;
string* oldResponse = &responseB;
sem_t* responseSemaphore;

void generateResponse()
{
  string content = scoreSummary(currentScores.mAllTime,
                                currentScores.mLastDay,
                                currentScores.mMostRecent);
  *oldResponse =
                  "HTTP/1.1 200 OK\r\n"
                  "Content-Type: application/json\r\n"
                  "Content-Length: ";
  *oldResponse += to_string(content.size()) + "\r\n\r\n";
  *oldResponse += content;
  for (int i = 0; i < SEMAPHORE_COUNT; i++)
  {
    sem_wait(responseSemaphore);
  }
  // response mutex/semaphore
  swap(oldResponse, currentResponse);
  for (int i = 0; i < SEMAPHORE_COUNT; i++)
    sem_post(responseSemaphore);
}

bool sort_score(const PlayerScore& a, const PlayerScore& b) {
  return a.mScore > b.mScore;
}

char last_day_pt[128];

void update_24_point()
{
  system_clock::time_point now = system_clock::now();
  now -= hours(24);
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  strftime(last_day_pt, sizeof(last_day_pt), "%FT%R", std::localtime(&t));
}

bool last_24(const PlayerScore& a)
{
  return a.mDateTime >= last_day_pt;
}

void initScores()
{
  update_24_point();
  
  PlayerScores allScores = getScores();
  currentScores.mAllScores = allScores;
  currentScores.mAllTime = getTop(allScores, RETURN_AMT, sort_score);
  currentScores.mLastDay = getTop(getSubset(allScores, last_24), RETURN_AMT, sort_score);
  currentScores.mMostRecent = getTop(allScores, RETURN_AMT, [](const PlayerScore& a, const PlayerScore& b) {
    return a.mDateTime > b.mDateTime;
  });
  generateResponse();
}

void addTopScore(PlayerScores* scores, PlayerScore score, int target_amount)
{
  auto top_pos = std::lower_bound(scores->begin(), scores->end(), score, [](const PlayerScore& a, const PlayerScore& b) {
    return a.mScore > b.mScore;
  });
  if (top_pos != scores->end())
  {
    scores->insert(top_pos, score);
    while (scores->size() > target_amount)
      scores->pop_back();
  }
}

void updateScores(PlayerScore s)
{
  if (pthread_mutex_lock(&newScoreMutex) == 0)
  {
    addTopScore(&currentScores.mAllTime, s, RETURN_AMT);
    update_24_point();
    currentScores.mLastDay = getSubset(currentScores.mLastDay, last_24);
    addTopScore(&currentScores.mLastDay, s, RETURN_AMT);
    currentScores.mMostRecent.push_front(s);
    currentScores.mMostRecent.pop_back();
    generateResponse();
    pthread_mutex_unlock(&newScoreMutex);
  }
}

static const char handled_char = ' ';
static void* handled = (void*) &handled_char;

static void *callback(enum mg_event event,
                      struct mg_connection *conn) {
  if (event == MG_NEW_REQUEST)
  {
    struct mg_request_info * req = mg_get_request_info(conn);
    if (!strcmp(req->uri, "/leaderboard/"))
    {
      if (!strcmp(req->request_method, "GET"))
      {
        sem_wait(responseSemaphore);
        // Make a copy, I don't know how much work happens in mg_printf, so copying allows us to ensure we don't
        // hold the semaphore open too long
        string response = *currentResponse;
        sem_post(responseSemaphore);
        mg_printf(conn, response.c_str());
      }
      if (!strcmp(req->request_method, "POST"))
      {
        char post_data[8192];
        char input1[128];
        char input2[128];
        char input3[128];
        char input4[128];

        int post_data_len;

        // Read POST data
        post_data_len = mg_read(conn, post_data, sizeof(post_data));

        // Parse form data. input1 and input2 are guaranteed to be NUL-terminated
        mg_get_enc_var(post_data, post_data_len, "initials", input1, sizeof(input1));
        mg_get_enc_var(post_data, post_data_len, "score", input2, sizeof(input2));
        mg_get_enc_var(post_data, post_data_len, "date", input3, sizeof(input3));
        mg_get_enc_var(post_data, post_data_len, "altar", input4, sizeof(input4));

        PlayerScore ps;
        bool error = false;
        try {
          ps.mInitials = input1;
          ps.mScore = stoi(input2);
          ps.mDateTime = input3;
          ps.mAltar = input4;
          updateScores(ps);
        } catch (...) {
          error = true;
        }

        if (!error)
        {
          {
            std::ifstream  src(document_root + "/images/cor.gif", std::ios::binary);
            std::ofstream  dst(document_root + "/scores/" + ps.toFilename(),   std::ios::binary);
            dst << src.rdbuf();
          }
          mg_printf(conn, "%s", "HTTP/1.0 200 OK\r\n");
          string tmp_dir = document_root + "/tmp";
          string tmp_file = tmp_dir + "/" + ps.toFilename();
          if (mg_upload_with_buf(conn, tmp_dir.c_str(), post_data, post_data_len, tmp_file.c_str()) > 0)
          {
            {
              std::ifstream  src(tmp_file, std::ios::binary);
              std::ofstream  dst(document_root + "/scores/" + ps.toFilename(),   std::ios::binary);
              dst << src.rdbuf();              
            }
            mg_printf(conn, "Content-Type: text/html\r\n\r\n");
            std::string response = "OK<br><img src=\"../scores/"+ps.toFilename()+"\">";
            mg_printf(conn, response.c_str(), mg_get_request_info(conn)->ev_data);
          }
        }
      }
      // Mark as processed
      return handled;
    }
    return NULL;
  }
  return NULL;
}

struct mg_context* smContext = NULL;

int main(int argc, const char * argv[])
{
  pthread_mutex_init(&newScoreMutex, NULL);
  sem_unlink("cor_response_semaphore");
  responseSemaphore = sem_open("cor_response_semaphore", O_CREAT, 0644, SEMAPHORE_COUNT);

  initScores();

  const char *options[] = {
    "listening_ports", "12084",
    "document_root", document_root.c_str(),
    NULL};
  smContext = mg_start(&callback, NULL, options);
  while (1) {
    usleep(100000);
  }

  pthread_mutex_destroy(&newScoreMutex);
  sem_destroy(responseSemaphore);

  return 0;
}