#include "server.hpp"


int main()
{
    gobang_server gs(HOST, USER, PWD, DBNAME, PORT);
    gs.start(8080);
    
    return 0;
}

// void test()
// {
//     // MYSQL* ps = util_mysql::mysql_create(HOST, USER, PWD, DBNAME, PORT);
//     // const char* q = "insert stu values(null , '小白', 18, 110, 112, 120);";
//     // util_mysql::mysql_exec(ps, q);
//     // util_mysql::mysql_destroy(ps);

//     Json::Value root;
//     root["姓名"] = "小明";
//     root["年龄"] = 18;
//     root["语文"] = 120;
//     root["数学"] = 110;
//     root["英语"] = 115;
//     root["成绩"].append("语文");
//     root["成绩"].append("数学");
//     root["成绩"].append("英语");
//     std::string value;
//     util_json::serialization(root, value);
//     LOG(DEBUG, "%s", value.c_str());

//     Json::Value v;
//     util_json::deserialization(value, v);
//     std::cout << "姓名：" << v["姓名"].asString() << std::endl;
//     std::cout << "年龄：" << v["年龄"].asInt() << std::endl;

//     std::cout << "成绩：\n";
//     for (int i = 0; i < v["成绩"].size(); ++i)
//     {
//         std::cout << v["成绩"][i].asString() << " : " << v[v["成绩"][i].asString()].asInt() << std::endl;
//     }
// }

// void onlinetest()
// {
//     WSserver::connection_ptr conn;
//     uint64_t id = 3;
//     onlineuser users;
//     users.enter_game_hall(id, conn);
//     LOG(DEBUG, "enter_game_hall");
//     users.exit_game_hall(id);
//     LOG(DEBUG, "exit_game_hall");

//     users.enter_game_room(id, conn);
//     LOG(DEBUG, "enter_game_room");

//     users.exit_game_room(id);
//     LOG(DEBUG, "exit_game_room");
// }

// void roomtest()
// {
//     // room_manager rm()
//     user_table ut(HOST, USER, PWD, DBNAME, PORT);
//     onlineuser om;
//     room_manager rm(&ut, &om);
//     rm.create_room(10, 20);
// }

// int main()
// {
//     //test();
//     // onlinetest();
//     roomtest();
//     return 0;
// }

