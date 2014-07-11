//
//  main.cpp
//  scoreboardserver
//
//  Created by Brian Richardson on 7/9/14.
//  Copyright (c) 2014 churchofrobotron. All rights reserved.
//

// TODO:  Photo
// TODO:  Date/time/machine

#include <string>
#include <deque>
#include <iostream>
#include <sstream>
#include <vector>

#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#include "dirent.h"

#include "mongoose.h"

using namespace std;

const int SEMAPHORE_COUNT = 16;

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

  string toJSON() const;
};

string PlayerScore::toJSON() const
{
  string ret = "{ ";
  ret += "\"initials\": \"" + mInitials + "\", ";
  ret += "\"score\": " + to_string(mScore);
  ret += " }";
  return ret;
}

typedef std::deque<PlayerScore> PlayerScores;

PlayerScores getScores()
{
  PlayerScores ret;

  DIR *dirp = opendir("/Users/bzztbomb/projects/churchOfRobotron/scoreboardserver/www/scores/");
  struct dirent * dp;
  while ((dp = readdir(dirp)) != NULL) {
    try {
      PlayerScore score;
      string filename = dp->d_name;
      if (filename == "." || filename == "..")
        continue;
      auto extPos = filename.rfind(".gif");
      if (extPos == string::npos)
        continue;
      filename.erase(extPos);
      vector<string> items = split(filename, '_');
      score.mInitials = items.size() ? items[0] : "";
      score.mScore = items.size() > 1 ? stoi(items[1]) : -1;
      ret.push_back(score);
    } catch (...) {
      //
    }
  }
  closedir(dirp);
  return ret;
}

PlayerScores getTop(PlayerScores scores, int howmany)
{
  sort(scores.begin(), scores.end(), [](const PlayerScore& a, const PlayerScore& b) {
    return a.mScore > b.mScore;
  });
  PlayerScores ret;
  for (int i = 0; i < min(howmany, (int) scores.size()); i++)
    ret.push_back(scores[i]);
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
  string ret = "{";

  ret += "\"alltime\" : \n" + scoresToJSON(allTime) + ",\n";
  ret += "\"lastday\" : \n" + scoresToJSON(lastDay) + ",\n";
  ret += "\"mostrecent\" : \n" + scoresToJSON(mostRecent);

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
    int res = sem_wait(responseSemaphore);
    int count;
    sem_getvalue(responseSemaphore, &count);
    printf("res: %d, count: %d\n", res, count);
  }
  // response mutex/semaphore
  swap(oldResponse, currentResponse);
  for (int i = 0; i < SEMAPHORE_COUNT; i++)
    sem_post(responseSemaphore);
}

void initScores()
{
  PlayerScores allScores = getScores();
  currentScores.mAllScores = allScores;
  currentScores.mAllTime = getTop(allScores, 20);
  currentScores.mLastDay = currentScores.mAllTime;
  currentScores.mMostRecent = currentScores.mAllTime;
  generateResponse();
}

void addTopScore(PlayerScores* scores, PlayerScore score)
{
  auto top_pos = std::lower_bound(scores->begin(), scores->end(), score, [](const PlayerScore& a, const PlayerScore& b) {
    return a.mScore > b.mScore;
  });
  if (top_pos != scores->end())
  {
    scores->insert(top_pos, score);
    scores->pop_back();
  }
}

void updateScores(PlayerScore s)
{
  if (pthread_mutex_lock(&newScoreMutex) == 0)
  {
    addTopScore(&currentScores.mAllTime, s);
    addTopScore(&currentScores.mLastDay, s);
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
        char post_data[8192],
        input1[sizeof(post_data)], input2[sizeof(post_data)];
        int post_data_len;

        // Read POST data
        post_data_len = mg_read(conn, post_data, sizeof(post_data));
        
        // Parse form data. input1 and input2 are guaranteed to be NUL-terminated
        mg_get_enc_var(post_data, post_data_len, "initials", input1, sizeof(input1));
        mg_get_enc_var(post_data, post_data_len, "score", input2, sizeof(input2));

        bool error = false;
        try {
          PlayerScore ps;
          ps.mInitials = input1;
          ps.mScore = stoi(input2);
          updateScores(ps);
        } catch (...) {
          error = true;
        }
        mg_printf(conn, "%s", "HTTP/1.0 200 OK\r\n\r\n");
        mg_upload_with_buf(conn, "/Users/bzztbomb/", post_data, post_data_len);
      }
      
      // Mark as processed
      return handled;
    }
    return NULL;
  } else if (event == MG_UPLOAD) {
    mg_printf(conn, "Saved [%s]", mg_get_request_info(conn)->ev_data);
    return handled;
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
    "document_root", "/Users/bzztbomb/projects/churchOfRobotron/scoreboardserver/www",
    NULL};
  smContext = mg_start(&callback, NULL, options);
  while (1) {
    usleep(100000);
  }

  pthread_mutex_destroy(&newScoreMutex);
  sem_destroy(responseSemaphore);

  return 0;
}