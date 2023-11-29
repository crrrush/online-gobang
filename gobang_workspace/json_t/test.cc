#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <jsoncpp/json/json.h>

void getvalue(Json::Value &root)
{
    root["姓名"] = "小明";
    root["年龄"] = 18;
    root["语文"] = 120;
    root["数学"] = 110;
    root["英语"] = 115;
    root["成绩"].append("语文");
    root["成绩"].append("数学");
    root["成绩"].append("英语");
    // root["成绩"].append(root["语文"]);
    // root["成绩"].append(root["数学"]);
    // root["成绩"].append(root["英语"]);
}

std::string serialization(Json::Value &v)
{
    // 1. 实例化一个StreamWriterBuilder对象
    Json::StreamWriterBuilder swb;
    // 2. 获得一个StreamWriter对象
    std::unique_ptr<Json::StreamWriter> psw(swb.newStreamWriter());
    // 3. 使用Json::Value对象，调用write接口写入数据
    std::stringstream ss;
    if (0 != psw->write(v, &ss))
    { /*error*/
    }

    return ss.str();
}

Json::Value deserialization(const std::string &s)
{
    // 1. 实例化一个CharReaderBuilder对象
    Json::CharReaderBuilder crb;
    // 2. 从工厂获取CharReader对象
    std::unique_ptr<Json::CharReader> pcr(crb.newCharReader());
    // 3. 反序列化获得Json::Value对象
    std::string err;
    Json::Value res;
    if (pcr->parse(s.c_str(), s.c_str() + s.size(), &res, &err) == false)
    { /*error*/
    }

    return res;
}

int main()
{
    Json::Value root;
    getvalue(root);

    std::string s = serialization(root);

    std::cout << s << std::endl;

    Json::Value v = deserialization(s);
    std::cout << "姓名：" << v["姓名"].asString() << std::endl;
    std::cout << "年龄：" << v["年龄"].asInt() << std::endl;

    std::cout << "成绩：\n";
    for (int i = 0; i < v["成绩"].size(); ++i)
    {
        std::cout << v["成绩"][i].asString() << " : " << v[v["成绩"][i].asString()].asInt() <<std::endl;
    }

    return 0;
}