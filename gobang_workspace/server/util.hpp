#pragma once

#include "log.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

#include <mysql/mysql.h>
#include <jsoncpp/json/json.h>

/*
* 对频繁使用的代码块封装
*/

// 数据库操作工具包
class util_mysql
{
public:
    // 创建数据库
    static MYSQL *mysql_create(const std::string &host,
                               const std::string &username,
                               const std::string &password,
                               const std::string &dbname,
                               uint16_t port = 3306)
    {
        MYSQL *mysql = mysql_init(nullptr);
        if (mysql == nullptr)
        {
            LOG(ERROR, "mysql init failed!");
            return nullptr;
        }
        // 2. 连接服务器
        if (mysql_real_connect(mysql,
                               host.c_str(),
                               username.c_str(),
                               password.c_str(),
                               dbname.c_str(), port, nullptr, 0) == nullptr)
        {
            LOG(ERROR, "connect mysql server failed : %s", mysql_error(mysql));
            mysql_close(mysql);
            return nullptr;
        }
        // 3. 设置客户端字符集
        if (mysql_set_character_set(mysql, "utf8") != 0)
        {
            LOG(ERROR, "set client character failed : %s", mysql_error(mysql));
            mysql_close(mysql);
            return nullptr;
        }
        return mysql;
    }

    // 数据库操作
    static bool mysql_exec(MYSQL *mysql, const std::string &sql)
    {
        int ret = mysql_query(mysql, sql.c_str());
        if (ret != 0)
        {
            LOG(ERROR, "%s\n", sql.c_str());
            LOG(ERROR, "mysql query failed : %s\n", mysql_error(mysql));
            return false;
        }
        return true;
    }

    // 关闭数据库
    static void mysql_destroy(MYSQL *mysql)
    {
        if (mysql != nullptr)
        {
            mysql_close(mysql);
        }
        return;
    }
};

// json处理工具包
class util_json
{
public:
    // json序列化 json格式数据转化成字符串
    static bool serialization(const Json::Value &v, std::string &str)
    {
        // 1. 实例化一个StreamWriterBuilder对象
        Json::StreamWriterBuilder swb;
        // 2. 获得一个StreamWriter对象
        std::unique_ptr<Json::StreamWriter> psw(swb.newStreamWriter());
        // 3. 使用Json::Value对象，调用write接口写入数据
        std::stringstream ss;
        if (0 != psw->write(v, &ss))
        {
            LOG(ERROR, "serialization failed!");
            return false;
        }
        str = ss.str();

        return true;
    }

    // json反序列化 字符串转化成json格式数据
    static bool deserialization(const std::string &s, Json::Value &v)
    {
        // 1. 实例化一个CharReaderBuilder对象
        Json::CharReaderBuilder crb;
        // 2. 从工厂获取CharReader对象
        std::unique_ptr<Json::CharReader> pcr(crb.newCharReader());
        // 3. 反序列化获得Json::Value对象
        std::string err;
        if (pcr->parse(s.c_str(), s.c_str() + s.size(), &v, &err) == false)
        {
            LOG(ERROR, "deserialization failed: %s", err.c_str());
            return false;
        }

        return true;
    }
};

// 字符串处理工具包
class util_string
{
public:
    // 字符串切割
    static int split(const std::string &src, const std::string &sep, std::vector<std::string> &res)
    {
        size_t pos, idx = 0;
        while (idx < src.size())
        {
            pos = src.find(sep, idx);
            if (pos == std::string::npos)
            {
                // 没有找到,字符串中没有间隔字符了，则跳出循环
                res.push_back(src.substr(idx));
                break;
            }
            if (pos == idx)
            {
                idx += sep.size();
                continue;
            }
            res.push_back(src.substr(idx, pos - idx));
            idx = pos + sep.size();
        }
        return res.size();
    }
};


// 文件读取工具包
class util_file
{
public:
    static bool read(const std::string &filename, std::string &body)
    {
        // 打开文件
        std::ifstream ifs(filename, std::ios::binary);
        if (ifs.is_open() == false)
        {
            LOG(ERROR, "%s file open failed!!", filename.c_str());
            return false;
        }
        // 获取文件大小
        size_t fsize = 0;
        ifs.seekg(0, std::ios::end);
        fsize = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        body.resize(fsize);
        // 将文件所有数据读取出来
        ifs.read(&body[0], fsize);
        if (ifs.good() == false)
        {
            LOG(ERROR, "read %s file content failed!", filename.c_str());
            ifs.close();
            return false;
        }
        // 关闭文件
        ifs.close();
        return true;
    }
};
