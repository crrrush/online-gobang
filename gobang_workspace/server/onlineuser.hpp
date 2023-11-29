#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

#include "log.hpp"
#include "util.hpp"

/*
* 在线用户管理模块
* 管理在线用户
* 用户登录成功后进入游戏大厅，对战匹配成功后进入游戏房间
* 
* 功能需求：
* 维护两个容器：游戏大厅和游戏房间
* 进入/退出 大厅/房间
* 判断用户是否在大厅/房间内
* 获取大厅/房间指定用户的连接
*/


class onlineuser
{
public:
    // onlineuser(/* args */);
    // ~onlineuser();

    // websocket连接建立的时候才会加入游戏大厅&游戏房间在线用户管理
    void enter_game_hall(uint64_t uid, websocketpp::server<websocketpp::config::asio>::connection_ptr &conn);
    void enter_game_room(uint64_t uid, websocketpp::server<websocketpp::config::asio>::connection_ptr &conn);

    // websocket连接断开的时候，才会移除游戏大厅&游戏房间在线用户管理
    void exit_game_hall(uint64_t uid);
    void exit_game_room(uint64_t uid);

    // 判断当前指定用户是否在游戏大厅/游戏房间
    bool is_in_game_hall(uint64_t uid);
    bool is_in_game_room(uint64_t uid);

    // 通过用户ID在游戏大厅/游戏房间用户管理中获取对应的通信连接
    websocketpp::server<websocketpp::config::asio>::connection_ptr get_conn_from_hall(uint64_t uid);
    websocketpp::server<websocketpp::config::asio>::connection_ptr get_conn_from_room(uint64_t uid);

private: /* data */
    std::mutex _mtx;
    std::unordered_map<uint64_t, websocketpp::server<websocketpp::config::asio>::connection_ptr> _hall;
    std::unordered_map<uint64_t, websocketpp::server<websocketpp::config::asio>::connection_ptr> _room;
};

// onlineuser::onlineuser(/* args */)
// {
// }

// onlineuser::~onlineuser()
// {
// }

// websocket连接建立的时候才会加入游戏大厅&游戏房间在线用户管理
void onlineuser::enter_game_hall(uint64_t uid, websocketpp::server<websocketpp::config::asio>::connection_ptr &conn)
{
    std::unique_lock<std::mutex> lock(_mtx);
    _hall.insert(std::make_pair(uid, conn));
}


void onlineuser::enter_game_room(uint64_t uid, websocketpp::server<websocketpp::config::asio>::connection_ptr &conn)
{
    std::unique_lock<std::mutex> lock(_mtx);
    _room.insert(std::make_pair(uid, conn));
}


// websocket连接断开的时候，才会移除游戏大厅&游戏房间在线用户管理
void onlineuser::exit_game_hall(uint64_t uid)
{
    std::unique_lock<std::mutex> lock(_mtx);
    _hall.erase(uid);
}


void onlineuser::exit_game_room(uint64_t uid)
{
    std::unique_lock<std::mutex> lock(_mtx);
    _room.erase(uid);
}


// 判断当前指定用户是否在游戏大厅/游戏房间
bool onlineuser::is_in_game_hall(uint64_t uid)
{
    std::unique_lock<std::mutex> lock(_mtx);
    auto it = _hall.find(uid);
    if (it == _hall.end())
    {
        return false;
    }
    return true;
}


bool onlineuser::is_in_game_room(uint64_t uid)
{
    std::unique_lock<std::mutex> lock(_mtx);
    auto it = _room.find(uid);
    if (it == _room.end())
    {
        return false;
    }
    return true;
}


// 通过用户ID在游戏大厅/游戏房间用户管理中获取对应的通信连接
websocketpp::server<websocketpp::config::asio>::connection_ptr onlineuser::get_conn_from_hall(uint64_t uid)
{
    std::unique_lock<std::mutex> lock(_mtx);
    auto it = _hall.find(uid);
    if (it == _hall.end())
    {
        return websocketpp::server<websocketpp::config::asio>::connection_ptr();
    }
    return it->second;
}


websocketpp::server<websocketpp::config::asio>::connection_ptr onlineuser::get_conn_from_room(uint64_t uid)
{
    std::unique_lock<std::mutex> lock(_mtx);
    auto it = _room.find(uid);
    if (it == _room.end())
    {
        return websocketpp::server<websocketpp::config::asio>::connection_ptr();
    }
    return it->second;
}
