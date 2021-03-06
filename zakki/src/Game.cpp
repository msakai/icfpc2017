#include <fstream>
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <random>
#include <memory>

#include "json.hpp"

#include "Game.hpp"
#include "Player.hpp"

using namespace std;
// for convenience
using json = nlohmann::json;

Map::Map(const JMap& m)
  : sites(m.sites),
    mines(m.mines),
    rivers(m.rivers.size()) {
  //cerr << "new Map" << endl;
  int size = sites[sites.size() - 1] + 1;
  connection.resize(size * size);
  riverIndex.resize(size * size);
  fill(riverIndex.begin(), riverIndex.end(), -1);
  distanceCache.resize(size * size);
  fill(distanceCache.begin(), distanceCache.end(), numeric_limits<int>::max());
  for (size_t i = 0; i < m.rivers.size(); i++) {
    auto r = m.rivers[i];
    connection[r.source * size  + r.target] = true;
    connection[r.target * size  + r.source] = true;
    riverIndex[r.source * size  + r.target] = i;
    riverIndex[r.target * size  + r.source] = i;
    }
  //cerr << "ok." << endl;
}

int Map::distance(int from, int to) const {
  const int max_site = sites[sites.size() - 1] + 1;
  const int size = sites.size();
  int offset = from * max_site;
  //vector<int> dist(sites.size(), no_route);
  if (distanceCache[offset + to] != no_route)
    return distanceCache[offset + to];
  distanceCache[offset + from] = 0;
  for (int n = 0; n < size; n++) {
    for (int i = 0; i < size; i++) {
      int  source = sites[i];
      if (distanceCache[offset + source] == no_route)
        continue;
      for (int j = 0; j < size; j++) {
        if (i == j)
          continue;
        int target = sites[j];
        if (connected(source, target)) {
          int d = distanceCache[offset + source] + 1;
          //if (j == to) return d;
          if (distanceCache[offset + target] > d) {
            //cerr << "distance:" << i << "->" << j << ":" << d << endl;
            distanceCache[offset + target] = d;
          }
        }
      }
    }
    if (distanceCache[offset + to] != no_route)
      return distanceCache[offset + to];
  }
  //if (distanceCache[to] != no_route)
  //return distanceCache[to];
  return 0;
}

int Game::score(int punter) {
  int score = 0;
  const int size = map.sites.size();
  for (int mine : map.mines) {
    vector<int> r = route(mine, punter);
    for (int i = 0; i < size; i++) {
      int source = map.sites[i];
      //if (mine != i && hasRoute(mine, source, punter)) {
      if (mine != source && r[source] != no_route) {
        int d = map.distance(mine, source);
        //cerr << mine << ":" << source << "->" << d << endl;
        score += d * d;
      }
    }
  }
  return score;
}

bool Game::hasRoute(int from, int to, int punter) {
  const int max_site = map.sites[map.sites.size() - 1] + 1;
  const int size = map.sites.size();
  vector<int> dist(max_site, no_route);
  dist[from] = 0;
  for (int n = 0; n < size; n++) {
    bool changed = false;
    for (int i = 0; i < size; i++) {
      int source = map.sites[i];
      if (dist[source] == no_route)
        continue;
      for (int j = 0; j < size; j++) {
        int target = map.sites[j];
        if (i == j || dist[target] < no_route)
          continue;
        if (map.connected(source, target) && owner[map.riverId(source, target)] == punter) {
          //cerr << "owenr " << i << "-" << j << ":" << map.riverId(i, j) << " " << owner[map.riverId(i, j)] << endl;
          if (target == to)
            return true;
          int d = dist[source] + 1;
          dist[target] = d;
          changed = true;
        }
      }
    }
    if (!changed) return false;
  }
  return false;
}

vector<int> Game::route(int from, int punter) {
  const int max_site = map.sites[map.sites.size() - 1] + 1;
  const int size = map.sites.size();
  vector<int> dist(max_site, no_route);
  dist[from] = 0;
  for (int n = 0; n < size; n++) {
    bool changed = false;
    for (int i = 0; i < size; i++) {
      int source = map.sites[i];
      if (dist[source] == no_route)
        continue;
      for (int j = 0; j < size; j++) {
        int target = map.sites[j];
        if (i == j || dist[target] < no_route)
          continue;
        if (map.connected(source, target) && owner[map.riverId(source, target)] == punter) {
          //cerr << "owenr " << i << "-" << j << ":" << map.riverId(i, j) << " " << owner[map.riverId(i, j)] << endl;
          int d = dist[source] + 1;
          dist[target] = d;
          changed = true;
        }
      }
    }
    if (!changed)
      break;
  }
  return dist;
}

json recvMessage() {
  int n;
  char c;
  cin >> n >> c;
  cerr << n << c << endl;
  if (c != ':')
    throw invalid_argument("bad prefix");
  std::string buf(n + 1, '\0');
  cin.read(&buf[0], n);
  return json::parse(buf);
}

void sendMessage(json json) {
  string str = json.dump();
  int n = str.size();
  cerr << "send:" << n << ":" << json;
  cout << n << ":" << str << flush;
}

JMap parseMap(json map) {
  JMap m;

  auto sites = map.at("sites");
  //cerr << sites << endl;
  assert(sites.is_array());

  for (auto it = sites.begin(); it != sites.end(); it++) {
    int id = it->at("id");
    //cerr << "site " << id << " " << m.sites.size() << endl;
    m.sites.push_back(id);
  }
  sort(m.sites.begin(), m.sites.end());
  //cerr << json(m.sites) << endl;

  auto rivers = map.at("rivers");
  //cerr << rivers << endl;
  assert(rivers.is_array());
  for (auto it = rivers.begin(); it != rivers.end(); it++) {
    int s = it->at("source");
    int t = it->at("target");
    //cerr << "river " << s << " -> " << t << endl;
    m.rivers.push_back(River(s, t));
  }

  auto mines = map.at("mines");
  //cerr << mines << endl;
  assert(mines.is_array());
  m.mines = mines.get<std::vector<int>>();
  //cerr << json(m.mines) << endl;

  return m;
}

JMove parseMove(json jmove) {
  auto pass = jmove.find("pass");
  if (pass != jmove.end()) {
    int punter = pass->at("punter");
    return JMove::pass(punter);
  }
  auto claim = jmove.find("claim");
  if (claim != jmove.end()) {
    int punter = claim->at("punter");
    int source = claim->at("source");
    int target = claim->at("target");
    return JMove::claim(punter, source, target);
  }
  throw invalid_argument("bad move");
}

json toJson(JMove move) {
  json msg;
  if (move.isPass()) {
    msg["pass"]["punter"] = move.punter;
  } else {
    msg["claim"]["punter"] = move.punter;
    msg["claim"]["source"] = move.source ;
    msg["claim"]["target"] = move.target;
  }
  return msg;
}

json toJson(const JMap& jmap) {
  json msg;
  for (auto id : jmap.sites) {
    json site;
    site["id"] = id;
    msg["sites"].push_back(site);
  }
  msg["mines"] = jmap.mines;
  for (auto r : jmap.rivers) {
    json jr;
    jr["source"] = r.source;
    jr["target"] = r.target;
    msg["rivers"].push_back(jr);
  }

  return msg;
}

json toJson(const Game& game) {
  json msg;
  msg["map"] = toJson(game.game.map);
  msg["punter"] = game.game.punter;
  msg["punters"] = game.game.punters;
  msg["owner"] = game.owner;
  msg["turn"] = game.turn;

  return msg;
}

vector<JMove> parseMoves(json jmove) {
  vector<JMove> moves;
  auto jmoves = jmove.at("moves");
  //cerr << jmoves << endl;
  for (auto it = jmoves.begin(); it != jmoves.end(); ++it) {
    //cerr << *it << endl;
    moves.push_back(parseMove(*it));
  }
  return moves;
}

JGame parseGame(json jsgame) {
  JGame game;
  auto jsmap = jsgame.at("map");
  cerr << jsmap.dump() << endl;
  game.map = parseMap(jsmap);
  //cerr << "map" << endl;
  game.punter = jsgame.at("punter");
  //cerr << game.punter << endl;
  game.punters = jsgame.at("punters");
  //cerr << game.punters << endl;
  return game;
}

void handshake()
{  json startMsg;
  startMsg["me"] = "SamBobR";
  sendMessage(startMsg);
}

void loop() {
  JGame jgame;// = parseGame(msg);
  shared_ptr<Game> game;//(jgame);

  while (true) {
    json msg = recvMessage();
    cerr << msg << endl;

    auto jyou = msg.find("you");
    if (jyou != msg.end()) {
      cerr << "YOU:" << *jyou << endl;
      continue;
    }
    auto jmap = msg.find("map");
    if (jmap != msg.end()) {
      cerr << "SETUP:" << *jmap << endl;
      //msg = recvMessage();
      //cerr << "parse Game" << endl;
      jgame = parseGame(msg);
      //cerr << "make Game" << endl;
      game = make_shared<Game>(jgame);
      //cerr << "ok" << endl;

      json readyMsg;
      readyMsg["ready"] = game->game.punter;
      readyMsg["state"] = toJson(*game);
      sendMessage(readyMsg);
      continue;
    }

    //json msg = recvMessage();
    auto jmove = msg.find("move");
    if (jmove != msg.end()) {
      auto jstate = msg.find("state");
      if (jstate != msg.end()) {
        jgame = parseGame(*jstate);
        game = make_shared<Game>(jgame);
        game->owner = (*jstate)["owner"].get<vector<int>>();
        game->turn = (*jstate)["turn"].get<int>();
      }

      auto ms = parseMoves(*jmove);
      //cerr << "moves " << ms.size() << endl;
      game->update(ms);
      auto move = toJson(genmove(*game, game->game.punter));
      move["state"] = toJson(*game);
      sendMessage(move);
      continue;
    }
    auto jstop = msg.find("stop");
    if (jstop != msg.end()) {
      break;
    }
  }
}

int main() {
  initPlayer();
#if 1
  try {
    cerr << "HANDSHAKE" << endl;
    handshake();
    loop();
  } catch (exception ex) {
    cerr << ex.what() << endl;
  }
  return 0;
#else
  string game_str = "{\"punter\":0,"
    "\"punters\":2,"
    "\"map\":{\"sites\":[{\"id\":4},{\"id\":1},{\"id\":3},{\"id\":6},{\"id\":5},{\"id\":0},{\"id\":7},{\"id\":2}],"
    "\"rivers\":[{\"source\":3,\"target\":4},{\"source\":0,\"target\":1},{\"source\":2,\"target\":3},"
    "{\"source\":1,\"target\":3},{\"source\":5,\"target\":6},{\"source\":4,\"target\":5},"
    "{\"source\":3,\"target\":5},{\"source\":6,\"target\":7},{\"source\":5,\"target\":7},"
    "{\"source\":1,\"target\":7},{\"source\":0,\"target\":7},{\"source\":1,\"target\":2}],"
    "\"mines\":[1,5]}}";
  cerr << game_str << endl;
  auto jsgame = json::parse(game_str);
  cerr << jsgame.dump() << endl;
  auto jsmap = jsgame["map"];
  cerr << jsmap.dump() << endl;
  auto jmap = parseMap(jsmap);

  string move_str = "{\"move\":{\"moves\":[{\"pass\":{\"punter\":0}},{\"pass\":{\"punter\":1}}]}}";
  auto jmove = json::parse(move_str);
  cerr << jmove << endl;
  string move_str2 = "{\"move\":{\"moves\":[{\"claim\":{\"punter\":0,\"source\":0,\"target\":1}},{\"claim\":{\"punter\":1,\"source\":1,\"target\":2}}]}}";
  auto jmove2 = json::parse(move_str2);
  cerr << jmove2 << endl;

  Map m{jmap};
  cerr << m.distance(1, 5) << endl;
#endif
}
