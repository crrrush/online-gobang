#include <stdio.h>
#include <string.h>
#include <mysql/mysql.h>

#define HOST "127.0.0.1"
#define PORT 3306
#define USER "root"
#define PWD ""
#define DBNAME "CRgobang"

int main()
{
    // 1.初始化mysql句柄
    MYSQL *mq = mysql_init(NULL);
    if (NULL == mq)
    {
        /*error*/
        exit(-1);
    }
    // 2.连接服务器
    if (mysql_real_connect(mq, HOST, USER, PWD, DBNAME, PORT, NULL, 0) == NULL)
    {
        /*error*/
        mysql_close(mq);
        exit(-1);
    }
    // 3.设置客户端字符集
    if (mysql_set_character_set(mq, "utf8") != 0)
    {
        /*error*/
        exit(-1);
    }
    // 4.选择要操作的数据库
    // int mysql_selectdb(mysql, dbname);
    // 已选择，可忽略

    // 5.执行sql语句
    // const char* q = "insert stu values(null , '小明', 18, 110, 112, 120);";
    // const char* q = "update stu set ch=ch+30 where sn=1;";
    // const char* q = "delete from stu where sn=1;";
    const char* q = "select * from stu;";

    if(mysql_query(mq, q) != 0)
    {
        /*error*/
        mysql_close(mq);
        exit(-1);
    }
    // 5.1 如果是查询语句，则需要保存结果到本地
    MYSQL_RES* res = mysql_store_result(mq);
    if(NULL == res)
    {
        /*error*/
        mysql_close(mq);
        exit(-1);
    }
    // 5.2 获取结果集中的结果条数
    int row = mysql_num_rows(res);
    int col = mysql_num_fields(res);
    // 5.3 遍历保持到本地的结果集
    for(int i = 0;i < row;++i)
    {
        MYSQL_ROW rows = mysql_fetch_row(res);
        for(int j = 0;j < col;j++) printf("%s\t",rows[j]);
        printf("\n");
    }
    // 5.4 释放结果集
    mysql_free_result(res);
    // 6. 关闭连接，释放句柄
    mysql_close(mq);
    return 0;
}