#pragma once

#include <unordered_map>

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

#include "log.hpp"
#include "util.hpp"

/*
* 为用户的连接维护一个session
* 根据session确认登录状态，用户端同步维护cookie信息
* 
* session记录用户的id以及自身的session id
* session与cookie一样都是需要根据不同情况设定不同的生命周期的，所以需要一个定时器（由websocket库提供）
* 
* session管理模块
* 增删查改session
* 更改session生命周期/状态
*/


typedef enum
{
    LOGOUT,
    LOGIN
} ss_statu;

class session
{
private:
    // 标识符
    uint64_t _ssid;
    // session对应的用户ID
    uint64_t _uid;
    // 用户状态：未登录，已登录
    ss_statu _statu;
    // session关联的定时器
    websocketpp::server<websocketpp::config::asio>::timer_ptr _tp; 
public:
    session(uint64_t ssid) : _ssid(ssid) { LOG(DEBUG, "SESSION %p 被创建！！", this); }
    ~session() { LOG(DEBUG, "SESSION %p 被释放！！", this); }

    uint64_t ssid() { return _ssid; } // 获取ssid

    void set_statu(ss_statu statu) { _statu = statu; } // 设置状态LOGOUT/LOGIN

    void set_user(uint64_t uid) { _uid = uid; } // 初始化uid

    uint64_t get_user() { return _uid; } // 获取用户uid

    bool is_login() { return (_statu == LOGIN); } // 判断是否在线

    void set_timer(const websocketpp::server<websocketpp::config::asio>::timer_ptr &tp) { _tp = tp; } // 初始化_tp

    websocketpp::server<websocketpp::config::asio>::timer_ptr &get_timer() { return _tp; } // 获取tp指针
};

#define SESSION_TIMEOUT 30000
#define SESSION_FOREVER -1
using session_ptr = std::shared_ptr<session>;

class session_manager
{
private:
    // session id分配
    uint64_t _next_ssid;
    // 互斥锁
    std::mutex _mutex;
    // uid ssid
    std::unordered_map<uint64_t, session_ptr> _session;
    // 定时器回指指针
    websocketpp::server<websocketpp::config::asio> *_server;

public:
    session_manager(websocketpp::server<websocketpp::config::asio> *srv) : _next_ssid(1), _server(srv) { LOG(DEBUG, "session管理器初始化完毕！"); }
    ~session_manager() { LOG(DEBUG, "session管理器即将销毁！"); }

    // 创建session
    session_ptr create_session(uint64_t uid, ss_statu statu)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        session_ptr ssp(new session(_next_ssid));
        ssp->set_statu(statu);
        ssp->set_user(uid);
        _session.insert(std::make_pair(_next_ssid, ssp));
        _next_ssid++;
        return ssp;
    }

    // 插入session
    void append_session(const session_ptr &ssp)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _session.insert(std::make_pair(ssp->ssid(), ssp));
    }

    // 通过ssid获取session_ptr
    session_ptr get_session_by_ssid(uint64_t ssid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _session.find(ssid);
        if (it == _session.end())
        {
            return session_ptr();
        }
        return it->second;
    }

    // 移除session
    void remove_session(uint64_t ssid)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _session.erase(ssid);
    }

    // 设置session存活时间
    // SESSION_TIMEOUT 30000
    // SESSION_FOREVER -1
    void set_session_expire_time(uint64_t ssid, int ms)
    {
        // 依赖于websocketpp的定时器来完成session生命周期的管理。
        //  登录之后，创建session，session需要在指定时间无通信后删除
        //  但是进入游戏大厅，或者游戏房间，这个session就应该永久存在
        //  等到退出游戏大厅，或者游戏房间，这个session应该被重新设置为临时，在长时间无通信后被删除
        session_ptr ssp = get_session_by_ssid(ssid);
        if (ssp.get() == nullptr)
        {
            return;
        }
        websocketpp::server<websocketpp::config::asio>::timer_ptr tp = ssp->get_timer();
        if (tp.get() == nullptr && ms == SESSION_FOREVER) // 1. 在session永久存在的情况下，设置永久存在
        {
            return;
        }
        else if (tp.get() == nullptr && ms != SESSION_FOREVER) // 2. 在session永久存在的情况下，设置指定时间之后被删除的定时任务
        {
            websocketpp::server<websocketpp::config::asio>::timer_ptr tmp_tp = _server->set_timer(ms, std::bind(&session_manager::remove_session, this, ssid));
            ssp->set_timer(tmp_tp);
        }
        else if (tp.get() != nullptr && ms == SESSION_FOREVER) // 3. 在session设置了定时删除的情况下，将session设置为永久存在
        {
            // 删除定时任务--- stready_timer删除定时任务会导致任务直接被执行
            tp->cancel(); // 因为这个取消定时任务并不是立即取消的
            // 因此重新给session管理器中，添加一个session信息, 且添加的时候需要使用定时器，而不是立即添加
            ssp->set_timer(websocketpp::server<websocketpp::config::asio>::timer_ptr()); // 将session关联的定时器设置为空
            _server->set_timer(0, std::bind(&session_manager::append_session, this, ssp));
        }
        else if (tp.get() != nullptr && ms != SESSION_FOREVER) // 4. 在session设置了定时删除的情况下，将session重置删除时间
        {
            tp->cancel();
            // 将session重新添加回去
            ssp->set_timer(websocketpp::server<websocketpp::config::asio>::timer_ptr()); // 将session关联的定时器设置为空
            _server->set_timer(0, std::bind(&session_manager::append_session, this, ssp));

            // 重新给session添加定时销毁任务
            websocketpp::server<websocketpp::config::asio>::timer_ptr tmp_tp = _server->set_timer(ms, std::bind(&session_manager::remove_session, this, ssp->ssid()));
            // 重新设置session关联的定时器
            ssp->set_timer(tmp_tp);
        }
    }
};