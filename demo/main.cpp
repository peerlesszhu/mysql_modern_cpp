/*
 * > Compile:
 * g++ -c -x c++ /root/main.cpp -I /usr/local/include -I /usr/include/mysql -g2 -gdwarf-2 -o "/root/mysql_demo.o" -Wall -Wswitch -W"no-deprecated-declarations" -W"empty-body" -Wconversion -W"return-type" -Wparentheses -W"no-format" -Wuninitialized -W"unreachable-code" -W"unused-function" -W"unused-value" -W"unused-variable" -O0 -fno-strict-aliasing -fno-omit-frame-pointer -fthreadsafe-statics -fexceptions -frtti -std=c++17
 * > Link:
 * g++ -o "/root/mysql_demo.out" -Wl,--no-undefined -Wl,-L/usr/local/lib -L/usr/lib64/mysql -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -lpthread -lrt -ldl /root/mysql_demo.o -lmysqlclient
 */

#include <cstdlib>
#include <cstdio>

#include <iostream>
#include <thread>

#include "mysql_modern_cpp.hpp"

#if defined(_MSC_VER)
#if defined(_DEBUG)
#pragma comment(lib,"libmysql.lib")
#else
#pragma comment(lib,"libmysql.lib")
#endif
#endif

using namespace std;

struct user
{
	std::string name;
	int age{};
	std::tm birth{};

	template <class Recordset>
	bool orm(Recordset & rs)
	{
		return rs(name, age, birth);
	}
};

int main(int argc, char* argv[])
{
#if defined(_WIN32)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		mysql::database db;
		db.connect("localhost", "root", "123456", "mir3_user");
		db.set_charset("gbk");

		(db << "show variables like 'character%';").set_fields_format("{:30}", "{}")
			>> [](std::string name, std::string value)
		{
			printf("%s %s\n", name.data(), value.data());
		};

		// R"()" �� c++ 11 �� raw string �﷨�������ַ�������ʱ��Ҫ����β��ӷ�б��
		db << R"(CREATE TABLE `tbl_user` (
			`name` VARCHAR(20) NOT NULL,
			`age` INT NULL DEFAULT NULL,
			`birth` DATETIME NULL DEFAULT NULL,
			PRIMARY KEY(`name`)
			)
			COLLATE = 'gbk_chinese_ci'
			ENGINE = InnoDB
			;)";

		// "db << ..." ���ֲ�������ʽ������һ����ʱ���� �������ʱ��������ʱ���������������Զ�ִ��sql���
		// ע�����������ִ��sql���ʱ������ִ��󲻻����ʾ������������catch����
		db << "insert into tbl_user (name,age) values (?, ?);"
			<< "admin"
			<< 102;
		db << "update tbl_user set age=?,birth=? where name=?;"
			<< nullptr
			<< nullptr
			<< "admin";
		db << "update tbl_user set age=?,birth=? where name=?;"
			<< 55
			<< "1990-03-14 15:15:15"
			<< "admin";

		user u;
		// ��ѯ���ݵ��Զ���ṹ����
		db << "select name,age,birth from tbl_user where name=?" << "admin" >> u;
		db.execute("select name,age,birth from tbl_user where name=?", "admin").fetch(u);
		db << "select name,age,birth from tbl_user" >> [](user u)
		{
			printf("%s %d\n", u.name.data(), u.age);
		};

		// �Զ���ṹ�����Ϣ��ӵ����ݿ���
		u.name += std::to_string(std::rand());
		db << "insert into tbl_user (name,age,birth) values (?,?,?)" << u;

		db << "delete from tbl_user  where name=?;"
			<< "���¾�������";

		// ֱ�ӵ��� db.execute ��ֱ��ִ��sql��� ������ִ�����Խ���ʾ������������catch����
		db.execute("insert into tbl_user values (?, ?, ?);", "���¾�������", 32, "2020-03-14 10:10:10");

		std::string name, age, birth;

		int count = 0;
		db << "select count(*) from tbl_user;"
			>> count;

		db << "select name from tbl_user where age=55;"
			>> name;

		std::tm tm_birth{}; // ����ȡ�������ڴ洢��c++���ԵĽṹ��tm��
		db << "select birth from tbl_user where name=?;"
			<< name
			>> tm_birth;

		const char * inject_name = "admin' or 1=1 or '1=1"; // sql ע��
		db << "select count(*) from tbl_user where name=?;"
			<< inject_name
			>> count;

		// ����ȡ�����ݴ洢���������� ���������ֱ�Ӳ������ݵĻ�����buffer
		// ����ʱ����Ҫ��auto rs = ���ַ�ʽ��recordset��ʱ������������
		// ����operator>>��������ʱ������������ binder ָ��ָ������ݾ��ǷǷ�����
		mysql::binder * binder = nullptr;
		auto rs = db << "select birth from tbl_user where name='admin';";
		rs >> binder;
		MYSQL_TIME * time = (MYSQL_TIME *)(binder->buffer.get());
		printf("%d-%d-%d %d:%d:%d\n", time->year, time->month, time->day, time->hour, time->minute, time->second);

		// ��ѯ�����ݺ�ֱ�ӵ��� lambda �ص��������ж��������ݣ��ͻ���ö��ٴ�
		(db << "select name,age,birth from tbl_user where age>?;")
			<< 10
			>> [](std::string_view name, int age, std::string birth)
		{
			printf("%s %d %s\n", name.data(), age, birth.data());
		};

		db << "select age,birth from tbl_user;"
			>> std::tie(age, birth);

		db.execute("select name,age,birth from tbl_user where name=?;", name) >>
			[](std::string_view name, int age, std::string birth)
		{
		};

		// set_fields_format �������÷��ص��ַ����ĸ�ʽ��ע��ֻ�з����ַ���ʱ��������
		// c++ 20 ��format�﷨
		// {:>15} ��ʾ�Ҷ��� ��ռ15���ַ��Ŀ��
		// {:04} ��ռ4���ַ��Ŀ�� �������4���ַ���ǰ�油0
		rs = (db << "select name,age,birth from tbl_user;");
		// �ܹ�select���������ݣ�����set_fields_format����Ҫ����������ʽ��Ϣ
		// ���Ե���set_fields_format���ø�ʽ����������˾ͱ����м��У��������ʽ��Ϣ
		// Ҳ���Բ�����set_fields_format����ʱ��ʹ��Ĭ�ϵĸ�ʽ
		rs.set_fields_format("{:>15}", "{:04}", "{:%Y-%m-%d %H:%M:%S}");
		auto rs2 = std::move(rs);
		// �����Լ���Ҫ��һ��һ�еĻ�ȡ����
		while (rs2.fetch(name, age, binder))
		{
			MYSQL_TIME * time = (MYSQL_TIME *)(binder->buffer.get());
			printf("%s %s %d-%d-%d\n", name.data(), age.data(), time->year, time->month, time->day);
		}

		std::tuple<int, std::string> tup;
		db << "select age,birth from tbl_user;"
			>> tup;
		auto &[_age, _birth] = tup;
		printf("%d %s\n", _age, birth.data());
	}
	catch(std::exception& e)
	{
		printf("%s\n", e.what());
	}

#if defined(_WIN32)
	system("pause");
#endif
	return 0;
}
