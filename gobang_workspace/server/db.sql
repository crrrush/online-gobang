-- drop database if exists CRgobang;
create database if not exists CRgobang;
use CRgobang;
create table if not exists user(
    id int primary key auto_increment,
    username varchar(32) unique key not null,
    password varchar(128) not null,
    score int,
    total_count int,
    win_count int
);