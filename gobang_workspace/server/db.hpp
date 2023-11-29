#pragma once

#include <mutex>
#include <cassert>

#include "log.hpp"
#include "util.hpp"

/*
* 用户数据管理模块
* 用户数据存储在MySQL数据库中
* 向数据库中读取或写入用户数据
* 
* 注册新增用户数据
* 登录验证用户数据
* 游戏对战结束更新用户数据
* 查询用户数据
*/

class user_table
{
private:
    MYSQL *_mysql;     // mysql操作句柄
    std::mutex _mutex; // 互斥锁保护数据库的访问操作
public:
    user_table(const std::string &host,
               const std::string &username,
               const std::string &password,
               const std::string &dbname,
               uint16_t port = 3306)
    {
        _mysql = util_mysql::mysql_create(host, username, password, dbname, port);
        assert(_mysql != nullptr);
    }
    
    ~user_table()
    {
        if (_mysql != nullptr)
        {
            util_mysql::mysql_destroy(_mysql);
            _mysql = nullptr;
        }
    }
    
    // 注册时新增用户
    bool insert(Json::Value &user)
    {
#define INSERT_USER "insert user values(null, '%s', password('%s'), 1000, 0, 0);"
        // sprintf(void *buf, char *format, ...)
        if (user["password"].isNull() || user["username"].isNull()) // 需要用户名以及用户密码
        {
            LOG(DEBUG, "INPUT PASSWORD OR USERNAME");
            return false;
        }
        char sql[4096] = {0};
        sprintf(sql, INSERT_USER, user["username"].asCString(), user["password"].asCString());
        bool ret = util_mysql::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            LOG(DEBUG, "insert user info failed!!\n");
            return false;
        }
        return true;
    }
    
    // 登录验证，并返回详细的用户信息
    bool login(Json::Value &user)
    {
        if (user["password"].isNull() || user["username"].isNull()) // 需要用户名以及密码
        {
            LOG(DEBUG, "INPUT PASSWORD OR");
            return false;
        }
        // 以用户名和密码共同作为查询过滤条件，查询到数据则表示用户名密码一致，没有信息则用户名密码错误
#define LOGIN_USER "select id, score, total_count, win_count from user where username='%s' and password=password('%s');"
        char sql[4096] = {0};
        sprintf(sql, LOGIN_USER, user["username"].asCString(), user["password"].asCString());
        MYSQL_RES *res = NULL;
        {
            std::unique_lock<std::mutex> lock(_mutex); // 查询时需要加锁保护
            bool ret = util_mysql::mysql_exec(_mysql, sql);
            if (ret == false)
            {
                LOG(DEBUG, "user login failed\n");
                return false;
            }
            // 按理说要么有数据，要么没有数据，就算有数据也只能有一条数据
            res = mysql_store_result(_mysql);
            if (res == NULL)
            {
                LOG(DEBUG, "have no login user info!");
                return false;
            }
        }
        int row_num = mysql_num_rows(res);
        if (row_num != 1)
        {
            LOG(DEBUG, "the user information queried is not unique!");
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64)std::stol(row[0]);
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);
        mysql_free_result(res);
        return true;
    }
    
    // 通过用户名获取用户信息
    bool select_by_name(const std::string &name, Json::Value &user)
    {
#define USER_BY_NAME "select id, score, total_count, win_count from user where username='%s';"
        char sql[4096] = {0};
        sprintf(sql, USER_BY_NAME, name.c_str());
        MYSQL_RES *res = NULL;
        {
            std::unique_lock<std::mutex> lock(_mutex); // 查询时需要加锁保护
            bool ret = util_mysql::mysql_exec(_mysql, sql);
            if (ret == false)
            {
                LOG(DEBUG, "get user by name failed!!\n");
                return false;
            }
            // 按理说要么有数据，要么没有数据，就算有数据也只能有一条数据
            res = mysql_store_result(_mysql);
            if (res == NULL)
            {
                LOG(DEBUG, "have no user info!!");
                return false;
            }
        }
        int row_num = mysql_num_rows(res);
        if (row_num != 1)
        {
            LOG(DEBUG, "the user information queried is not unique!!");
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64)std::stol(row[0]);
        user["username"] = name;
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);
        mysql_free_result(res);
        return true;
    }
    
    // 通过用户名获取用户信息
    bool select_by_id(uint64_t id, Json::Value &user)
    {
#define USER_BY_ID "select username, score, total_count, win_count from user where id=%d;"
        char sql[4096] = {0};
        sprintf(sql, USER_BY_ID, id);
        MYSQL_RES *res = NULL;
        {
            std::unique_lock<std::mutex> lock(_mutex); // 查询时需要加锁保护
            bool ret = util_mysql::mysql_exec(_mysql, sql);
            if (ret == false)
            {
                LOG(DEBUG, "get user by id failed!!\n");
                return false;
            }
            // 按理说要么有数据，要么没有数据，就算有数据也只能有一条数据
            res = mysql_store_result(_mysql);
            if (res == NULL)
            {
                LOG(DEBUG, "have no user info!!");
                return false;
            }
        }
        int row_num = mysql_num_rows(res);
        if (row_num != 1)
        {
            LOG(DEBUG, "the user information queried is not unique!!");
            return false;
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        user["id"] = (Json::UInt64)id;
        user["username"] = row[0];
        user["score"] = (Json::UInt64)std::stol(row[1]);
        user["total_count"] = std::stoi(row[2]);
        user["win_count"] = std::stoi(row[3]);
        mysql_free_result(res);
        return true;
    }
    
    // 胜利时天梯分数增加30分，战斗场次增加1，胜利场次增加1
    bool win(uint64_t id)
    {
#define USER_WIN "update user set score=score+500, total_count=total_count+1, win_count=win_count+1 where id=%d;"
        char sql[4096] = {0};
        sprintf(sql, USER_WIN, id);
        bool ret = util_mysql::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            LOG(DEBUG, "update win user info failed!!\n");
            return false;
        }
        return true;
    }

    // 失败时天梯分数减少30，战斗场次增加1，其他不变
    bool lose(uint64_t id)
    {
#define USER_LOSE "update user set score=score-500, total_count=total_count+1 where id=%d;"
#define TO_ZERO "update user set score=0, total_count=total_count+1 where id=%d;"
        char sql[4096] = {0};

        // 判断是否会扣减分数至负数
        Json::Value user;
        select_by_id(id, user);
        if(user["score"].asUInt64() > 500)
        {
            sprintf(sql, USER_LOSE, id);
        }
        else
        {
            LOG(ERROR, "分数被降至最低分");
            sprintf(sql, TO_ZERO, id);
        }

        bool ret = util_mysql::mysql_exec(_mysql, sql);
        if (ret == false)
        {
            LOG(DEBUG, "update lose user info failed!!\n");
            return false;
        }
        return true;
    }
};