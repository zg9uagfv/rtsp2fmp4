#include "fmp4_server.h"
#include <thread>
#include <iostream>
#include "live555.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <string>
#include <locale>
#include <codecvt>
#include <fstream>
#include  <direct.h> 


using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using namespace websocketpp::http::parser;

std::string getConfig() {
	
	char db[255];
	_getcwd(db, 255);
	std::string path = db;
	path += "/config.json";

	std::wifstream f(path, std::ifstream::in);
	f.imbue(std::locale(std::locale("chs"), new std::codecvt_utf8<wchar_t>()));
	std::wstring ws;
	wchar_t c;
	while (f.get(c))
	{
		ws += c;
	}

	std::wstring_convert<std::codecvt<wchar_t, char, mbstate_t>> conv(new std::codecvt<wchar_t, char, mbstate_t>("chs"));
	std::string s = conv.to_bytes(ws);

	return s;
}


FMp4Server::FMp4Server(UsageEnvironment* env, int port) {

	std::string cfg = getConfig();

	rapidjson::Document jsonDoc;
	jsonDoc.Parse(cfg.data());
	rapidjson::Value vs = jsonDoc.GetArray();
	for (int i = 0; i < vs.Size(); i++)
	{
		const rapidjson::Value& v = vs[i];
		proxy.insert(std::pair<std::string, std::string>(v["source"].GetString(), v["target"].GetString()));
		//printf("%s, %s\n", v["source"].GetString(), v["target"].GetString());
	}

	this->env = env;

	m_server.set_access_channels(websocketpp::log::alevel::none);
	m_server.clear_access_channels(websocketpp::log::alevel::none);
	m_server.init_asio();

	m_server.set_open_handler(bind(&FMp4Server::on_open, this, ::_1));
	m_server.set_close_handler(bind(&FMp4Server::on_close, this, ::_1));
	m_server.set_message_handler(bind(&FMp4Server::on_message, this, ::_1, ::_2));
	m_server.listen(port);
	m_server.start_accept();
}

void FMp4Server::run_sync() {
	m_server.run();
	return;
}
void FMp4Server::run_async() {
	std::thread t1(bind(&server::run, &m_server));
	t1.detach();
	return;
}

void FMp4Server::stop() {
	m_server.stop();
}

void FMp4Server::on_open(connection_hdl hdl) {
	request req = m_server.get_con_from_hdl(hdl)->get_request();
	std::string uri = req.get_uri();
	std::string url = proxy.at(uri);
	boolean found = false;
	for (auto rc : m_rcs) {
		if (url == rc->url()) {
			found = true;
			rc->fmp4Server = this;
			FMp4Muxer muxer;
			rc->conns.insert(std::pair<connection_hdl, FMp4Muxer>(hdl, muxer));
		}
	}
	if (!found) {
		MyRTSPClient* rc = MyRTSPClient::createNew(*env, url.data());
		if (rc) {
			rc->connect();
			rc->fmp4Server = this;
			FMp4Muxer muxer;
			rc->conns.insert(std::pair<connection_hdl, FMp4Muxer>(hdl, muxer));
			m_rcs.insert(rc);
		}
	}
	//m_server.send(hdl, fmp4_header, fmp4_header_size, websocketpp::frame::opcode::BINARY);

}

void FMp4Server::on_close(connection_hdl hdl) {

	for (auto rc : m_rcs) {
		if (rc->conns.find(hdl) != rc->conns.end()) {
			rc->conns.erase(hdl);
		}
	}
	//std::set <MyRTSPClient*>::iterator it;
	for (std::set <MyRTSPClient*>::iterator it = m_rcs.begin(); it != m_rcs.end();)
	{
		if ((*it)->conns.size() == 0)
			m_rcs.erase(it++);
		else
			it++;
	}

}

void FMp4Server::on_message(connection_hdl hdl, server::message_ptr msg) {
	/*
	if (msg->get_payload() == "fmp4") {
		for (auto it : m_connections) {
			std::cout << "fmp4 : " << m_connections.size() << "\n";
			m_server.send(hdl, fmp4_header, fmp4_header_size, websocketpp::frame::opcode::BINARY);
		}
	}
	*/
}

void FMp4Server::send(connection_hdl hdl, uint8_t * buf, int buf_size)
{
	try
	{
		m_server.send(hdl, buf, buf_size, websocketpp::frame::opcode::BINARY);
	}
	catch (std::exception & e)
	{
		std::cout << e.what() << std::endl;
	}

}


/*

FMp4Server wss(9002);

void fmp4_server_batch_send(uint8_t* data, int data_size)
{
	wss.batch_send(data, data_size);
}

void fmp4_server_start() {
	std::thread t1(bind(&server::run, &wss.m_server));
	t1.detach();
	return;
}

void fmp4_server_stop() {
	wss.stop();
}
*/
