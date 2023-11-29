#pragma once

#include <iostream>
#include <string>
#include <functional>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

#include "log.hpp"
#include "util.hpp"
#include "db.hpp"
#include "onlineuser.hpp"
#include "room.hpp"
#include "session.hpp"
#include "matcher.hpp"

#define HOST "127.0.0.1"
#define PORT 3306
#define USER "root"
#define PWD "123123123"
#define DBNAME "CRgobang"


#define WWWROOT "./wwwroot/"

using WSserver = websocketpp::server<websocketpp::config::asio>;

class gobang_server
{
private:
    std::string _web_root; // 静态资源根目录 ./wwwroot/      /register.html ->  ./wwwroot/register.html
    WSserver _wssrv;
    user_table _ut;
    onlineuser _ou;
    room_manager _rm;
    matcher _mm;
    session_manager _sm;

private:
    // http 处理静态资源请求
    void file_handler(WSserver::connection_ptr &conn)
    {
        // 1. 获取到请求uri-资源路径，了解客户端请求的页面文件名称
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();
        // 2. 组合出文件的实际路径   相对根目录 + uri
        std::string path = _web_root + uri;
        // 3. 如果请求的是根目录，增加一个后缀  login.html,    /  ->  /login.html
        if (path.back() == '/')
        {
            path += "login.html";
        }
        // 4. 读取文件内容
        Json::Value resp_json;
        std::string body;
        bool ret = util_file::read(path, body);
        if (ret == false) //  4.1 文件不存在，读取文件内容失败，返回404
        {
            util_file::read("./wwwroot/404.html", body);
            conn->set_status(websocketpp::http::status_code::not_found);
            conn->set_body(body);
            return;
        }
        // 5. 设置响应正文
        conn->set_body(body);
        conn->set_status(websocketpp::http::status_code::ok);
    }

    // 发送http响应 websocket连接指针，处理结果，响应码，原因
    void http_resp(WSserver::connection_ptr &conn, bool result, websocketpp::http::status_code::value code, const std::string &reason)
    {
        Json::Value resp_json;
        resp_json["result"] = result;
        resp_json["reason"] = reason;
        std::string resp_body;
        util_json::serialization(resp_json, resp_body);
        conn->set_status(code);
        conn->set_body(resp_body);
        conn->append_header("Content-Type", "application/json");
    }

    // http 处理用户注册请求
    void reg(WSserver::connection_ptr &conn)
    {
        websocketpp::http::parser::request req = conn->get_request();
        // 1. 获取到请求正文
        std::string req_body = conn->get_request_body();
        // 2. 对正文进行json反序列化，得到用户名和密码
        Json::Value reg_info;
        bool ret = util_json::deserialization(req_body, reg_info);
        if (ret == false)
        {
            LOG(DEBUG, "反序列化注册信息失败");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请求的正文格式错误");
        }
        // 3. 进行数据库的用户新增操作
        if (reg_info["username"].isNull() || reg_info["password"].isNull())
        {
            LOG(DEBUG, "用户名或密码缺失");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请输入用户名/密码");
        }
        ret = _ut.insert(reg_info);
        if (ret == false)
        {
            LOG(DEBUG, "向数据库插入数据失败");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "用户名已经被占用!");
        }
        //  如果成功了，则返回200
        http_resp(conn, true, websocketpp::http::status_code::ok, "注册用户成功");
    }
    
    // http 处理用户登录请求
    void login(WSserver::connection_ptr &conn)
    {
        // 1. 获取请求正文，并进行json反序列化，得到用户名和密码
        std::string req_body = conn->get_request_body();
        Json::Value login_info;
        bool ret = util_json::deserialization(req_body, login_info);
        if (ret == false)
        {
            LOG(DEBUG, "反序列化登录信息失败");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请求的正文格式错误");
        }
        // 2. 校验正文完整性，进行数据库的用户信息验证
        if (login_info["username"].isNull() || login_info["password"].isNull())
        {
            LOG(DEBUG, "用户名或密码缺失");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "请输入用户名/密码");
        }
        ret = _ut.login(login_info);
        //  2.1 如果验证失败，则返回400
        if (ret == false)
        {
            LOG(DEBUG, "用户名密码错误");
            return http_resp(conn, false, websocketpp::http::status_code::bad_request, "用户名密码错误");
        }
        //  2.2 如果验证成功，给客户端创建session
        uint64_t uid = login_info["id"].asUInt64();
        session_ptr ssp = _sm.create_session(uid, LOGIN);
        if (ssp.get() == nullptr)
        {
            LOG(DEBUG, "创建会话失败");
            return http_resp(conn, false, websocketpp::http::status_code::internal_server_error, "创建会话失败");
        }
        _sm.set_session_expire_time(ssp->ssid(), SESSION_TIMEOUT);
        // 3. 设置响应头部：Set-Cookie,将sessionid通过cookie返回
        std::string cookie_ssid = "SSID=" + std::to_string(ssp->ssid());
        conn->append_header("Set-Cookie", cookie_ssid);
        http_resp(conn, true, websocketpp::http::status_code::ok, "登录成功");
    }
    
    // 从cookie字段中获取session id
    bool get_cookie_val(const std::string &cookie_str, const std::string &key, std::string &val)
    {
        // Cookie: SSID=XXX; path=/;
        // 1. 以; 作为间隔，对字符串进行分割，得到各个单个的cookie信息
        std::string sep = "; ";
        std::vector<std::string> cookie_arr;
        util_string::split(cookie_str, sep, cookie_arr);
        for (auto str : cookie_arr)
        {
            // 2. 对单个cookie字符串，以 = 为间隔进行分割，得到key和val
            std::vector<std::string> tmp_arr;
            util_string::split(str, "=", tmp_arr);
            if (tmp_arr.size() != 2)
            {
                continue;
            }
            if (tmp_arr[0] == key)
            {
                val = tmp_arr[1];
                return true;
            }
        }
        return false;
    }
    
    // http 处理用户信息获取请求
    void info(WSserver::connection_ptr &conn)
    {
        Json::Value err_resp;
        // 1. 获取请求信息中的Cookie，从Cookie中获取ssid
        std::string cookie = conn->get_request_header("Cookie");
        if (cookie.empty())
        {
            // 如果没有cookie，返回错误：没有cookie信息，让客户端重新登录
            return http_resp(conn, true, websocketpp::http::status_code::bad_request, "找不到cookie信息，请重新登录");
        }
        //  1.1 从cookie中取出ssid
        std::string ssid;
        bool ret = get_cookie_val(cookie, "SSID", ssid);
        if (ret == false)
        {
            // cookie中没有ssid，返回错误：没有ssid信息，让客户端重新登录
            return http_resp(conn, true, websocketpp::http::status_code::bad_request, "找不到ssid信息，请重新登录");
        }
        // 2. 在session管理中查找对应的会话信息
        session_ptr ssp = _sm.get_session_by_ssid(std::stol(ssid));
        if (ssp.get() == nullptr)
        {
            // 没有找到session，则认为登录已经过期，需要重新登录
            return http_resp(conn, true, websocketpp::http::status_code::bad_request, "登录过期，请重新登录");
        }
        // 3. 从数据库中取出用户信息，进行序列化发送给客户端
        uint64_t uid = ssp->get_user();
        Json::Value user_info;
        ret = _ut.select_by_id(uid, user_info);
        if (ret == false)
        {
            // 获取用户信息失败，返回错误：找不到用户信息
            return http_resp(conn, true, websocketpp::http::status_code::bad_request, "找不到用户信息，请重新登录");
        }
        std::string body;
        util_json::serialization(user_info, body);
        conn->set_body(body);
        conn->append_header("Content-Type", "application/json"); // http_resp?
        conn->set_status(websocketpp::http::status_code::ok);
        // 4. 刷新session的过期时间
        _sm.set_session_expire_time(ssp->ssid(), SESSION_TIMEOUT);
    }
    
//////////////////// http请求响应函数
    void http_callback(websocketpp::connection_hdl hdl)
    {
        WSserver::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string method = req.get_method();
        std::string uri = req.get_uri();
        if (method == "POST" && uri == "/reg")
        {
            reg(conn); // 用户注册请求
        }
        else if (method == "POST" && uri == "/login")
        {
            login(conn); // 用户登录请求
        }
        else if (method == "GET" && uri == "/info")
        {
            info(conn); // 用户信息获取请求
        }
        else
        {
            return file_handler(conn); // 页面静态资源请求
        }
    }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    // 响应函数
    void ws_resp(WSserver::connection_ptr conn, Json::Value &resp)
    {
        std::string body;
        util_json::serialization(resp, body);
        conn->send(body);
    }
    
    // 使用cookie信息找到session
    session_ptr get_session_by_cookie(WSserver::connection_ptr conn)
    {
        Json::Value err_resp;
        // 1. 获取请求信息中的Cookie，从Cookie中获取ssid
        std::string cookie_str = conn->get_request_header("Cookie");
        if (cookie_str.empty())
        {
            // 1.1 如果没有cookie，返回错误：没有cookie信息，让客户端重新登录
            err_resp["optype"] = "hall_ready";
            err_resp["reason"] = "没有找到cookie信息，需要重新登录";
            err_resp["result"] = false;
            ws_resp(conn, err_resp);
            return session_ptr();
        }
        // 2. 从cookie中取出ssid
        std::string ssid_str;
        bool ret = get_cookie_val(cookie_str, "SSID", ssid_str);
        if (ret == false)
        {
            // 2.1 cookie中没有ssid，返回错误：没有ssid信息，让客户端重新登录
            err_resp["optype"] = "hall_ready";
            err_resp["reason"] = "没有找到SSID信息，需要重新登录";
            err_resp["result"] = false;
            ws_resp(conn, err_resp);
            return session_ptr();
        }
        // 3. 在session管理中查找对应的会话信息
        session_ptr ssp = _sm.get_session_by_ssid(std::stol(ssid_str));
        if (ssp.get() == nullptr)
        {
            // 3.1 没有找到session，则认为登录已经过期，需要重新登录
            err_resp["optype"] = "hall_ready";
            err_resp["reason"] = "没有找到session信息，需要重新登录";
            err_resp["result"] = false;
            ws_resp(conn, err_resp);
            return session_ptr();
        }
        return ssp;
    }
    
    // 建立游戏大厅长连接
    void wsopen_game_hall(WSserver::connection_ptr conn)
    {
        Json::Value resp_json;
        resp_json["optype"] = "hall_ready"; // 响应类型
        // 1. 登录验证--判断当前客户端是否已经成功登录
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }
        // 2. 判断当前客户端是否是重复登录
        if (_ou.is_in_game_hall(ssp->get_user()) || _ou.is_in_game_room(ssp->get_user()))
        {
            resp_json["reason"] = "玩家重复登录！";
            resp_json["result"] = false;
            return ws_resp(conn, resp_json);
        }
        // 3. 将当前客户端以及连接加入到游戏大厅
        _ou.enter_game_hall(ssp->get_user(), conn);
        // 4. 给客户端响应游戏大厅连接建立成功
        resp_json["result"] = true;
        ws_resp(conn, resp_json);
        // 5. 记得将session设置为永久存在
        _sm.set_session_expire_time(ssp->ssid(), SESSION_FOREVER);
    }
    
    // 建立游戏房间长连接
    void wsopen_game_room(WSserver::connection_ptr conn)
    {
        Json::Value resp_json;
        resp_json["optype"] = "room_ready"; // 响应类型
        // 1. 获取当前客户端的session 验证是否成功登录
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr) return;
        
        // 2. 当前用户是否已经在在线用户管理的游戏房间或者游戏大厅中---在线用户管理
        if (_ou.is_in_game_hall(ssp->get_user()) || _ou.is_in_game_room(ssp->get_user()))
        {
            resp_json["reason"] = "玩家重复登录！";
            resp_json["result"] = false;
            return ws_resp(conn, resp_json);
        }

        // 3. 判断当前用户是否已经创建好了房间 --- 房间管理
        room_ptr rp = _rm.get_room_by_uid(ssp->get_user());
        if (rp.get() == nullptr)
        {
            resp_json["reason"] = "没有找到玩家的房间信息";
            resp_json["result"] = false;
            return ws_resp(conn, resp_json);
        }

        // 4. 将当前用户添加到在线用户管理的游戏房间中
        _ou.enter_game_room(ssp->get_user(), conn);

        // 5. 将session重新设置为永久存在
        _sm.set_session_expire_time(ssp->ssid(), SESSION_FOREVER);

        // 6. 回复房间准备完毕
        resp_json["result"] = true;
        resp_json["room_id"] = (Json::UInt64)rp->id();
        resp_json["uid"] = (Json::UInt64)ssp->get_user();
        resp_json["white_id"] = (Json::UInt64)rp->get_white_user();
        resp_json["black_id"] = (Json::UInt64)rp->get_black_user();
        return ws_resp(conn, resp_json);
    }

/////////////////// Websocket长连接建立请求响应函数
    void wsopen_callback(websocketpp::connection_hdl hdl)
    {
        // websocket长连接建立成功之后的处理函数
        WSserver::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();
        if (uri == "/hall")
        {
            return wsopen_game_hall(conn); // 建立游戏大厅的长连接
        }
        else if (uri == "/room")
        {
            return wsopen_game_room(conn); // 建立游戏房间的长连接
        }
    }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // 处理断开游戏大厅长连接的请求
    void wsclose_game_hall(WSserver::connection_ptr conn)
    {
        // 1. 登录验证--判断当前客户端是否是登录状态
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }
        // 2. 将玩家从游戏大厅中移除
        _ou.exit_game_hall(ssp->get_user());
        // 3. 将session恢复生命周期的管理，设置定时销毁
        _sm.set_session_expire_time(ssp->ssid(), SESSION_TIMEOUT);
    }
    
    // 处理断开游戏房间长连接的请求
    void wsclose_game_room(WSserver::connection_ptr conn)
    {
        // 1. 获取会话信息，识别客户端登录状态
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            return;
        }
        // 2. 将玩家从在线用户管理中移除
        _ou.exit_game_room(ssp->get_user());
        // 3. 将session回复生命周期的管理，设置定时销毁
        _sm.set_session_expire_time(ssp->ssid(), SESSION_TIMEOUT);
        // 4. 将玩家从游戏房间中移除，房间中所有用户退出了就会销毁房间
        _rm.remove_room_user(ssp->get_user());
    }

/////////////////// Websocket长连接关闭请求响应函数
    void wsclose_callback(websocketpp::connection_hdl hdl)
    {
        // websocket连接断开前的处理
        WSserver::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();
        if (uri == "/hall") 
        {
            return wsclose_game_hall(conn); // 关闭游戏大厅长连接
        }
        else if (uri == "/room")
        {
            return wsclose_game_room(conn); // 关闭游戏房间长连接
        }
    }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // 处理大厅中的请求  开始匹配和取消匹配
    void wsmsg_game_hall(WSserver::connection_ptr conn, WSserver::message_ptr msg)
    {
        Json::Value resp_json;
        std::string resp_body;
        // 1. 身份验证，当前客户端到底是哪个玩家
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr) return;

        // 2. 获取请求信息
        std::string req_body = msg->get_payload();
        Json::Value req_json;
        bool ret = util_json::deserialization(req_body, req_json);
        if (ret == false)
        {
            resp_json["result"] = false;
            resp_json["reason"] = "请求信息解析失败";
            return ws_resp(conn, resp_json);
        }

        // 3. 对于请求进行处理：
        if (!req_json["optype"].isNull() && req_json["optype"].asString() == "match_start")
        {
            //  开始对战匹配：通过匹配模块，将用户添加到匹配队列中
            _mm.add(ssp->get_user());
            resp_json["optype"] = "match_start";
            resp_json["result"] = true;
            return ws_resp(conn, resp_json);
        }
        else if (!req_json["optype"].isNull() && req_json["optype"].asString() == "match_stop")
        {
            //  停止对战匹配：通过匹配模块，将用户从匹配队列中移除
            _mm.del(ssp->get_user());  // ??匹配成功时取消匹配？目前会照常匹配
            resp_json["optype"] = "match_stop";
            resp_json["result"] = true;
            return ws_resp(conn, resp_json);
        }
        resp_json["optype"] = "unknown";
        resp_json["reason"] = "请求类型未知";
        resp_json["result"] = false;
        return ws_resp(conn, resp_json);
    }

    // 处理房间中的请求  确认客户端信息，反序列化成json格式的信息交给上层room对象处理请求
    void wsmsg_game_room(WSserver::connection_ptr conn, WSserver::message_ptr msg)
    {
        Json::Value resp_json;
        // 1. 获取客户端session，识别客户端身份
        session_ptr ssp = get_session_by_cookie(conn);
        if (ssp.get() == nullptr)
        {
            LOG(DEBUG, "房间-没有找到会话信息");
            return;
        }

        // 2. 获取客户端房间信息
        room_ptr rp = _rm.get_room_by_uid(ssp->get_user());
        if (rp.get() == nullptr)
        {
            resp_json["optype"] = "unknow";
            resp_json["reason"] = "没有找到玩家的房间信息";
            resp_json["result"] = false;
            LOG(DEBUG, "房间-没有找到玩家房间信息");
            return ws_resp(conn, resp_json);
        }

        // 3. 对消息进行反序列化
        Json::Value req_json;
        std::string req_body = msg->get_payload();
        bool ret = util_json::deserialization(req_body, req_json);
        if (ret == false)
        {
            resp_json["optype"] = "unknow";
            resp_json["reason"] = "请求解析失败";
            resp_json["result"] = false;
            LOG(DEBUG, "房间-反序列化请求失败");
            return ws_resp(conn, resp_json);
        }

        // 4. 通过房间模块进行消息请求的处理
        rp->handle_request(req_json);
    }

/////////////////// Websocket长连接通信响应函数
    void wsmsg_callback(websocketpp::connection_hdl hdl, WSserver::message_ptr msg)
    {
        WSserver::connection_ptr conn = _wssrv.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req = conn->get_request();
        std::string uri = req.get_uri();
        if (uri == "/hall")
        {
            return wsmsg_game_hall(conn, msg); // 建立游戏大厅长连接
        }
        else if (uri == "/room")
        {
            return wsmsg_game_room(conn, msg); // 建立游戏房间长连接
        }
    }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

public:
    // mysql数据库访问模块初始化，以及服务器回调函数的设置
    gobang_server(const std::string &host,
                  const std::string &user,
                  const std::string &pass,
                  const std::string &dbname,
                  uint16_t port = PORT,
                  const std::string &wwwroot = WWWROOT) 
                  : _web_root(wwwroot), _ut(host, user, pass, dbname, port), _rm(&_ut, &_ou), _sm(&_wssrv), _mm(&_rm, &_ut, &_ou)
    {
        _wssrv.set_access_channels(websocketpp::log::alevel::none);
        _wssrv.init_asio();
        _wssrv.set_reuse_addr(true);
        
        // http请求响应函数
        _wssrv.set_http_handler(std::bind(&gobang_server::http_callback, this, std::placeholders::_1));
        // Websocket长连接建立请求响应函数
        _wssrv.set_open_handler(std::bind(&gobang_server::wsopen_callback, this, std::placeholders::_1));
        // Websocket长连接关闭请求响应函数
        _wssrv.set_close_handler(std::bind(&gobang_server::wsclose_callback, this, std::placeholders::_1));
        // Websocket通信响应函数
        _wssrv.set_message_handler(std::bind(&gobang_server::wsmsg_callback, this, std::placeholders::_1, std::placeholders::_2));
    }
    
    // 启动服务器
    void start(int port)
    {
        _wssrv.listen(port);
        _wssrv.start_accept();
        _wssrv.run();
    }
};