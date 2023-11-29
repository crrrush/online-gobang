

// using WSserver = websocketpp::server<websocketpp::config::asio>;

// void http_callback(WSserver *svr, websocketpp::connection_hdl chdl)
// {
//     WSserver::connection_ptr conptr = svr->get_con_from_hdl(chdl);
//     std::cout << "body: " << conptr->get_request_body() << std::endl;
//     websocketpp::http::parser::request req = conptr->get_request();
//     std::cout << "method: " << req.get_method() << std::endl;
//     std::cout << "uri: " << req.get_uri() << std::endl;

//     std::string body = "<html><body><h1>Hello world!</h1></body></html>";
//     conptr->set_body(body);
//     conptr->append_header("Content-Type", "text/html");
//     conptr->set_status(websocketpp::http::status_code::ok);
// }
// void open_callback(WSserver *svr, websocketpp::connection_hdl chdl)
// {
//     std::cout << "websocketpp握手成功！！！\n";
// }
// void close_callback(WSserver *svr, websocketpp::connection_hdl chdl)
// {
//     std::cout << "websocketpp断开连接\n";
// }
// void message_callback(WSserver *svr, websocketpp::connection_hdl chdl, WSserver::message_ptr msg)
// {
//     WSserver::connection_ptr conptr = svr->get_con_from_hdl(chdl);
//     std::cout << "ws message: " << msg->get_payload() << std::endl;
//     std::string resp = "sever response: " + msg->get_payload();
//     conptr->send(resp, websocketpp::frame::opcode::text);
// }

// int main()
// {
//     // 1. 实例化server对象
//     WSserver server;
//     // 2. 设置日志等级
//     server.set_access_channels(websocketpp::log::alevel::none);
//     // 3. 初始化asio调度器
//     server.init_asio();
//     server.set_reuse_addr(true);
//     // 4. 设置回调函数
//     server.set_http_handler(std::bind(http_callback, &server, std::placeholders::_1));
//     server.set_open_handler(std::bind(open_callback, &server, std::placeholders::_1));
//     server.set_close_handler(std::bind(close_callback, &server, std::placeholders::_1));
//     server.set_message_handler(std::bind(message_callback, &server, std::placeholders::_1, std::placeholders::_2));
//     // 5. 设置监听端口
//     server.listen(8080);
//     // 6. 获取新链接
//     server.start_accept();
//     // 7. 启动服务器
//     server.run();
//     return 0;
// }
