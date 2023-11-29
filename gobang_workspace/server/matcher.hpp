#pragma once

#include <list>
#include <mutex>
#include <condition_variable>

#include "util.hpp"
#include "onlineuser.hpp"
#include "db.hpp"
#include "room.hpp"

/*
* 对战匹配功能模块 
* 目前只存在积分匹配排位赛
* 相同段位的人进入相同匹配队列 不同段位的人进入不同的匹配队列
* 
* 使用一个阻塞队列实现匹配队列
* 创建线程从对应的匹配队列取出玩家进行对战
*
* 向外提供add接口添加玩家到匹配队列
* del接口取消对应玩家的匹配
*/

template <class T>
class match_queue
{
private:
    // 用链表而不直接使用queue是因为我们有中间删除数据的需要
    std::list<T> _list;
    // 实现线程安全
    std::mutex _mutex;
    // 这个条件变量主要为了阻塞消费者，后边使用的时候：队列中元素个数<2则阻塞
    std::condition_variable _cond;

public:
    // 获取元素个数
    int size()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _list.size();
    }

    // 判断是否为空
    bool empty()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        return _list.empty();
    }

    // 阻塞线程
    void wait()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cond.wait(lock);
    }

    // 入队数据，并唤醒线程
    void push(const T &data)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _list.push_back(data);
        _cond.notify_all();
    }

    // 出队数据
    bool pop(T &data)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_list.empty() == true)
            return false;

        data = _list.front();
        _list.pop_front();
        return true;
    }

    // 从阻塞队列中移除指定的数据
    void remove(T &data)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _list.remove(data);
    }
};

class matcher
{
private:
    // 第一分段选手匹配队列
    match_queue<uint64_t> _q_normal;
    // 第二分段匹配队列
    match_queue<uint64_t> _q_high;
    // 第三分段匹配队列
    match_queue<uint64_t> _q_super;

    // 对应三个匹配队列的处理线程
    std::thread _th_normal;
    std::thread _th_high;
    std::thread _th_super;

    // 房间管理模块
    room_manager *_rm;
    // 用户数据模块
    user_table *_ut;
    // 在线用户管理模块
    onlineuser *_ou;

private:
    // 新线程处理对应匹配队列的匹配从队列中取出玩家进行对战
    void handle_match(match_queue<uint64_t> &mq)//  match_queue<T>?
    {
        while (1)
        {
            // 1. 判断队列人数是否大于2，<2则阻塞等待
            while (mq.size() < 2)
            {
                mq.wait();
            }
            // 2. 走下来代表人数够了，出队两个玩家
            uint64_t uid1, uid2;
            bool ret = mq.pop(uid1);
            if (ret == false)
            {
                continue;
            }
            ret = mq.pop(uid2);
            if (ret == false)
            {
                add(uid1);
                continue;
            }
            // 3. 校验两个玩家是否在线，如果有人掉线，则将另一个人重新添加入队列
            websocketpp::server<websocketpp::config::asio>::connection_ptr conn1 = _ou->get_conn_from_hall(uid1);
            if (conn1.get() == nullptr)
            {
                add(uid2);
                LOG(INFO, "玩家%d掉线，重新匹配", uid1);
                continue;
            }
            websocketpp::server<websocketpp::config::asio>::connection_ptr conn2 = _ou->get_conn_from_hall(uid2);
            if (conn2.get() == nullptr)
            {
                add(uid1);
                LOG(INFO, "玩家%d掉线，重新匹配", uid2);
                continue;
            }
            // 4. 为两个玩家创建房间，并将玩家加入房间中
            room_ptr rp = _rm->create_room(uid1, uid2);
            if (rp.get() == nullptr)
            {
                add(uid1);
                add(uid2);
                continue;
            }
            // 5. 对两个玩家进行响应
            Json::Value resp;
            resp["optype"] = "match_success";
            resp["result"] = true;
            std::string body;
            util_json::serialization(resp, body);
            conn1->send(body);
            conn2->send(body);
        }
    }

    void th_normal_entry() { handle_match(_q_normal); } // 第一梯队匹配队列处理函数
    void th_high_entry() { handle_match(_q_high); }     // 第二梯队匹配队列处理函数
    void th_super_entry() { handle_match(_q_super); }   // 第三梯队匹配队列处理函数

public:
    // 房间管理模块 用户数据模块 在线用户管理模块
    matcher(room_manager *rm, user_table *ut, onlineuser *om) : _rm(rm), _ut(ut), _ou(om),
                                                                _th_normal(std::thread(&matcher::th_normal_entry, this)),
                                                                _th_high(std::thread(&matcher::th_high_entry, this)),
                                                                _th_super(std::thread(&matcher::th_super_entry, this))
    {
        LOG(DEBUG, "游戏匹配模块初始化完毕....");
    }

    // 输入uid 根据玩家的天梯分数，来判定玩家档次，添加到不同的匹配队列
    bool add(uint64_t uid)
    {
        //  1. 根据用户ID，获取玩家信息
        Json::Value user;
        bool ret = _ut->select_by_id(uid, user); // 检查是否存在
        if (ret == false)
        {
            LOG(DEBUG, "匹配时获取玩家:%d 信息失败！！", uid);
            return false;
        }
        int score = user["score"].asInt();

        // 2. 添加到指定的队列中
        if (score < 2000)
        {
            _q_normal.push(uid);
        }
        else if (score >= 2000 && score < 3000)
        {
            _q_high.push(uid);
        }
        else
        {
            _q_super.push(uid);
        }
        return true;
    }

    // 输入uid 取消匹配，从对应的匹配的匹配队列中移除
    bool del(uint64_t uid)
    {
        //  1. 根据用户ID，获取玩家信息
        Json::Value user;
        bool ret = _ut->select_by_id(uid, user);
        if (ret == false)
        {
            LOG(DEBUG, "取消匹配时获取玩家:%d 信息失败！！", uid);
            return false;
        }
        int score = user["score"].asInt();
        // 2. 从指定的队列中删除
        if (score < 2000)
        {
            _q_normal.remove(uid);
        }
        else if (score >= 2000 && score < 3000)
        {
            _q_high.remove(uid);
        }
        else
        {
            _q_super.remove(uid);
        }
        return true;
    }
};