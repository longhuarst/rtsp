// boost_rtsp_client.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>



#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <boost/regex.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
class tcpclient{


public:

	tcpclient(std::string name, boost::asio::io_service &service)
		:
		service_(service),
		name_("[ "+name+" ] --->. ")
	{
	
	}

	~tcpclient() {
	
	}




	bool connect_sync(std::string ip, int port)
	{
		boost::system::error_code error_code;

		socket_.reset(new boost::asio::ip::tcp::socket(service_));

		boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(ip), port);

		socket_->connect(endpoint,error_code);

		if (error_code) {

			std::cout<<name_<<"Connection Failed:"<<error_code.message() <<std::endl;

			socket_->close();
			return false;
		}

		std::cout << name_ << "Connection Suceed!" << std::endl;
		return true;

	}

	void connect_async(std::string ip, int port,boost::function<void (boost::system::error_code)> handler)
	{
		boost::system::error_code error_code;

		socket_.reset(new boost::asio::ip::tcp::socket(service_));

		boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(ip), port);

		socket_->async_connect(endpoint, handler);
	}

	void close() 
	{
		socket_->close();
	}

	size_t write_some_sync(std::string &msg) 
	{
		return socket_->write_some(boost::asio::buffer(msg));
	}

	void write_some_async(std::string &msg, boost::function<void (const boost::system::error_code& error, std::size_t bytes_transferred)> handler) {
		socket_->async_write_some(boost::asio::buffer(msg), handler);
	}

	size_t read_until_sync(std::string regex, boost::asio::streambuf &stream) {
		return boost::asio::read_until(*socket_, stream, boost::regex(regex));
	}


	void read_until_async(std::string regex, boost::asio::streambuf& stream, boost::function<void (const boost::system::error_code& error, size_t bytes_transferred)> handler) {
		boost::asio::async_read_until(*socket_, stream, boost::regex(regex), handler);
	}



	size_t read_sync(char *buffer, size_t length)
	{
		return boost::asio::read(*socket_, boost::asio::buffer(buffer,length));
	}

	void read_async(char* buffer, size_t length, boost::function<void (const boost::system::error_code& error, size_t bytes_transferred)> handler)
	{
		boost::asio::async_read(*socket_, boost::asio::buffer(buffer, length), handler);
	}

	size_t write_sync(char* buffer, size_t length)
	{
		return boost::asio::write(*socket_, boost::asio::buffer(buffer, length));
	}
	
	void write_async(char* buffer, size_t length, boost::function<void(const boost::system::error_code& error, size_t bytes_transferred)> handler)
	{
		boost::asio::async_write(*socket_, boost::asio::buffer(buffer, length), handler);
	}







private:
	boost::shared_ptr<boost::asio::ip::tcp::socket> socket_;
	boost::asio::io_service &service_;
	std::string name_;
};





class rtsp {


public:
	
	rtsp(std::string name, boost::asio::io_service& service)
		:
		name_("[ " + name + " ] --->. "),
		service_(service)
	{
		
	}


	~rtsp() 
	{
	
	}




	void start(std::string ip, int port)
	{
		ip_ = ip;
		port_ = port;
		rtsp_url_ = "rtsp://" + ip_ + ":" + std::to_string(port_) + "/h264";

		start();
	}


private:
	std::string name_;
	boost::shared_ptr<tcpclient> client_;
	boost::asio::io_service &service_;
	boost::shared_ptr<boost::asio::deadline_timer> reconnect_timer_;//重新连接定时器
	boost::shared_ptr<boost::asio::deadline_timer> connecting_timer_;//连接超时定时器
	std::string ip_;
	int port_;
	std::string rtsp_url_;

	void start()
	{
		//异步连接服务器
		client_.reset(new tcpclient(name_ + "::" + "tcpclient", service_));

		client_->connect_async(ip_, port_, boost::bind(&rtsp::connect_handler, this, boost::asio::placeholders::error));

		//同时启动异步定时器检查连接超时
		connecting_timer_.reset(new boost::asio::deadline_timer(service_, boost::posix_time::seconds(1)));

		connecting_timer_->async_wait(boost::bind(&rtsp::connecting_timer_handler, this, boost::asio::placeholders::error));

	}

	void connecting_timer_handler(const boost::system::error_code& error)
	{
		if (error) {
			std::cout << name_ << "Connecting Timer Failed:" << error.message() << std::endl;

			return;
		}

		std::cout << "连接超时..." << std::endl;

		client_->close();


		start();//重新开启
	}

	void connection_lost_handler()
	{
		//注册异步定时器， 等待重新连接
		client_->close();

		reconnect_timer_.reset(new boost::asio::deadline_timer(service_, boost::posix_time::seconds(1)));

		reconnect_timer_->async_wait(boost::bind(&rtsp::reconnect_timer_handler, this, boost::asio::placeholders::error));

	}


	void connect_handler(const boost::system::error_code& error)
	{

		connecting_timer_.reset(new boost::asio::deadline_timer(service_, boost::posix_time::seconds(1)));

		if (error) {
			std::cout << name_ << "Connection Failed:" << error.message() << std::endl;

			
			connection_lost_handler();

			return;
		}
		std::cout << name_ << "Connection Suceed!" << std::endl;

		options();//发起option操作
	}

	void reconnect_timer_handler(const boost::system::error_code& error)
	{
		if (error) {
			std::cout << name_ << "Reconnect Timer Failed:" << error.message() << std::endl;
			
			return;
		}

		std::cout << "准备 尝试重新连接...." << std::endl;

		start();//重新开启
	}






	
	std::string option_message;

	void options() {


		//if (stop_) {
		//	std::cout << "执行 options() 操作时检测到 stop_ 信号,不执行" << std::endl;
		//	teardown();//结束当前会话
		//	return;
		//}


		std::cout << name_ << "options()" << std::endl;

		option_message = "OPTIONS " + rtsp_url_ + " RTSP/1.0\r\nCSeq: 2\r\nUser-Agent: LibVLC/3.0.3 (LIVE555 Streaming Media v2016.11.28)\r\n\r\n";

		client_->write_some_async(option_message, boost::bind(&rtsp::option_write_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

	}


	boost::asio::streambuf options_streambuf;
	void option_write_handler(const boost::system::error_code &error, size_t bytes_transferred)
	{

		
		if (error) {
			std::cout << name_ << "option() Write Error:" << error.message() << std::endl;

			return;
		}

		options_streambuf.consume(options_streambuf.size());

		client_->read_until_async("\r\n\r\n", options_streambuf, boost::bind(&rtsp::options_read_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}


	void options_read_handler(const boost::system::error_code& error, size_t bytes_transferred)
	{

		if (error) {
			std::cout << name_ << "option() Read Error:" << error.message() << std::endl;

			return;
		}

		

		boost::asio::streambuf::const_buffers_type cbt = options_streambuf.data();

		std::string respone(boost::asio::buffers_begin(cbt), boost::asio::buffers_end(cbt));
		std::cout << name_ << "option() Read: { " << std::endl;
		std::cout << respone << std::endl;
		std::cout  << "};" << std::endl;


		describe();
	}


	std::string describe_message;
	void describe() {
		std::cout << name_ << "describe()" << std::endl;

		describe_message = "DESCRIBE " + rtsp_url_ + " RTSP/1.0\r\nCSeq: 3\r\nUser-Agent: LibVLC/3.0.3 (LIVE555 Streaming Media v2016.11.28)\r\nAccept: application/dsp\r\n\r\n";

		client_->write_some_async(describe_message, boost::bind(&rtsp::describe_write_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

	}

	boost::asio::streambuf describe_streambuf1;
	boost::asio::streambuf describe_streambuf2;
	int track_id_;
	int describe_head_length_;
	int describe_length_;
	char* describe_buffer_;

	void describe_write_handler(const boost::system::error_code& error, size_t bytes_transferred)
	{


		if (error) {
			std::cout << name_ << "describe() Write Error:" << error.message() << std::endl;

			return;
		}

		
		track_id_ = -1;
		describe_head_length_ = 0;
		describe_length_ = 0;
		describe_streambuf1.consume(describe_streambuf1.size());
		describe_streambuf2.consume(describe_streambuf2.size());

		client_->read_until_async("\r\n\r\n", describe_streambuf1, boost::bind(&rtsp::describe_read_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}

	

	
	void describe_read_handler(const boost::system::error_code& error, size_t bytes_transferred)
	{

		if (error) {
			std::cout << name_ << "describe() Read Error:" << error.message() << std::endl;

			return;
		}
		describe_head_length_ = bytes_transferred;

		char buffer[1024] = { 0 };
		describe_streambuf1.sgetn(buffer, bytes_transferred);//此处获取到的是track和其之前的数据 实际的数据还在之后的缓存中，需要寻找\r\n






		std::cout << name_ << "describe() Read1: ["<< bytes_transferred <<"]{ " << std::endl;
		std::cout << buffer << std::endl;
		std::cout << "};" << std::endl;

		
		//else {
			//std::cout << name_ << "describe() Read: " << "describe 操作中存在未编写的功能代码，再读一次！" << std::endl;

			std::string find_string(buffer);

			int pos = find_string.find("Content-Length:");

			std::string content = find_string.substr(pos + strlen("Content-Length:"));

			int size = content.size();
			for (int i = 0; i < size; ++i) {
				if (content[0] == ' ') {
					content = content.substr(1);
				}
			}

			size = content.size();
			for (int i = 0; i < size; ++i) {
				if (content[i] >= '0' && content[i] <= '9') {
				}
				else {
					content = content.substr(0,i);
					break;
				}
			}

			//std::cout << content <<std::endl;
			try
			{
				describe_length_ = track_id_ = boost::lexical_cast<int>(content);
			}
			catch (const std::exception& e)
			{
				std::cout << name_ << "describe() Read1: " <<  " 转换错误 "<<e.what()<<std::endl;

				connection_lost_handler();
				return;
			}
			

			//std::cout << "length = " << length << std::endl;
			//pos = content.find("\r\n");

			describe_buffer_ = new char[describe_length_];
			memset( describe_buffer_, 0, describe_length_);
			describe_read_counter_ = 0;

			int buffer_size = describe_streambuf1.size();
			describe_streambuf1.sgetn(describe_buffer_, buffer_size);//此处获取到的是track和其之前的数据 实际的数据还在之后的缓存中，需要寻找\r\n


			std::string test(describe_buffer_);

			std::cout << test << std::endl;

			describe_read_counter_ = buffer_size;

			client_->read_async(&describe_buffer_[buffer_size], describe_length_ -  describe_read_counter_, boost::bind(&rtsp::describe_read2_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
			//client_->read_until_async("\r\n", describe_streambuf1, boost::bind(&rtsp::describe_read_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
			//exit(-1);
		//}
	}

	int describe_read_counter_;
	void describe_read2_handler(const boost::system::error_code& error, size_t bytes_transferred)
	{

		if (error) {
			std::cout << name_ << "describe() Read Error:" << error.message() << std::endl;

			return;
		}

		

		describe_read_counter_ += bytes_transferred;
		if (describe_read_counter_ < describe_length_) {
			client_->read_async(&describe_buffer_[describe_read_counter_], describe_length_- describe_read_counter_, boost::bind(&rtsp::describe_read2_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
			return;
		}
		else {
			

			std::cout << name_ << "describe() Read2: [" << bytes_transferred << "]{ " << std::endl;
			std::cout << describe_buffer_ << std::endl;
			std::cout << "};" << std::endl;


			if (describe_buffer_[describe_length_ - 1] == '\n' && describe_buffer_[describe_length_ - 2] == '\r') {
				std::string track_id_str(describe_buffer_);
				size_t pos = track_id_str.find("track");
				track_id_str = track_id_str.substr(pos+strlen("track"));
				pos = track_id_str.find("\r\n");
				track_id_str = track_id_str.substr(0, pos);

				track_id_ = boost::lexical_cast<int>(track_id_str);

				delete[] describe_buffer_;
				setup();
			}
			else {
				std::cout << name_ << "describe() Read: " << "describe 操作中存在未编写的功能代码，再读一次！" << std::endl;
				exit(-1);
			}
		}


	}


	std::string setup_message;
	void setup() {

		std::cout << name_ << "setup()" << std::endl;

		setup_message = "SETUP " + rtsp_url_ + "?mode=Live&stream=0&buffer=-1&seek=0&fps=1&metainfo=&follow=0/track" + std::to_string(track_id_) + " RTSP/1.0\r\nCSeq: 4\r\nUser-Agent: LibVLC/3.0.3 (LIVE555 Streaming Media v2016.11.28)\r\nTTransport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n\r\n";

		client_->write_some_async(setup_message, boost::bind(&rtsp::setup_write_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

	}

	boost::asio::streambuf setup_streambuf;
	void setup_write_handler(const boost::system::error_code& error, size_t bytes_transferred)
	{


		if (error) {
			std::cout << name_ << "setup() Write Error:" << error.message() << std::endl;

			return;
		}

		client_->read_until_async("\r\n\r\n", setup_streambuf, boost::bind(&rtsp::setup_read_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}

	std::string session_;

	void setup_read_handler(const boost::system::error_code& error, size_t bytes_transferred) {


		if (error) {
			std::cout << name_ << "setup() Read Error:" << error.message() << std::endl;

			return;
		}


		char buffer[1024] = { 0 };

		setup_streambuf.sgetn(buffer, bytes_transferred);

		std::cout << name_ << "setup() Read: { " << std::endl;
		std::cout << buffer << std::endl;
		std::cout << "};" << std::endl;

		//查找session

		std::string session_str(buffer);

		size_t pos = session_str.find("Session:");
		session_ = session_str.substr(pos, session_str.length() - pos);
		size_t pos_end = session_.find("\r\n");
		session_ = session_.substr(0, pos_end);
		play();



	}


	std::string play_message;

	void play() {
		std::cout << name_ << "play()" << std::endl;

		play_message = "PLAY " + rtsp_url_ + "?mode=Live&stream=0&buffer=-1&seek=0&fps=1&metainfo=&follow=0/" + " RTSP/1.0\r\nCSeq: 5\r\nUser-Agent: LibVLC/3.0.3 (LIVE555 Streaming Media v2016.11.28)\r\n" + session_ + "\r\nRange: npt=0.000-\r\n\r\n";

		client_->write_some_async(play_message, boost::bind(&rtsp::play_write_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

	}

	boost::asio::streambuf play_streambuf;
	void play_write_handler(const boost::system::error_code& error, size_t bytes_transferred)
	{


		if (error) {
			std::cout << name_ << "play() Write Error:" << error.message() << std::endl;

			return;
		}

		client_->read_until_async("\r\n\r\n", play_streambuf, boost::bind(&rtsp::play_read_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}



	void play_read_handler(const boost::system::error_code& error, size_t bytes_transferred) {


		if (error) {
			std::cout << name_ << "play() Read Error:" << error.message() << std::endl;

			return;
		}


		char buffer[1024] = { 0 };

		play_streambuf.sgetn(buffer, bytes_transferred);

		std::cout << name_ << "paly() Read: { " << std::endl;
		std::cout << buffer << std::endl;
		std::cout << "};" << std::endl;

		std::cout << name_ << "paly() Read " << "start to sync packet!" << std::endl;
		sync_head();//开始同步rtsp数据包



	}




	char sync_head_;//同步头

	void sync_head() {

		//if (stop_) {
		//	std::cout << "执行 sync_head() 操作时检测到 stop_ 信号,不执行" << std::endl;
		//	teardown();//结束当前会话
		//	return;
		//}
		//std::cout<<"sync_head()"<<std::endl;
		client_->read_async(&sync_head_, 1, boost::bind(&rtsp::sync_head_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
		//boost::asio::async_read(socket_, boost::asio::buffer(&sync_head_, 1), boost::bind(&rtsp::sync_head_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}


	void sync_head_handler(const boost::system::error_code& error, size_t bytes_transferred) {
		if (error) {
			std::cerr << "sync_head_handler():" << boost::system::system_error(error).what() << std::endl;
			
			std::cout << "启动异步定时器重连..." << std::endl;
			connection_lost_handler();

			return;
		}
		//std::cout<<"收到数据["<<bytes_transferred<<"]-->"<<std::endl;
		//std::cout<<sync_head_<<std::endl;
		if (sync_head_ == '$') {
			//同步成功
			//std::cout<<"同步成功!"<<std::endl;
			sync_head3();//继续同步后三字节
		}
		else {
			//同步失败 继续同步
			std::cout << "同步失败！" << std::endl;
			sync_head();
		}
	}



	char sync_head3_[3];//同步头后三字节
	int sync_head3_type_;
	int sync_head3_length_;

	void sync_head3() {

		//if (stop_) {
		//	std::cout << "执行 sync_head3() 操作时检测到 stop_ 信号,不执行" << std::endl;
		//	teardown();//结束当前会话
		//	return;
		//}
		client_->read_async(sync_head3_,3, boost::bind(&rtsp::sync_head3_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

		/*boost::asio::async_read(socket_, boost::asio::buffer(sync_head3_, 3), boost::bind(&rtsp::sync_head3_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));*/
	}

	void sync_head3_handler(const boost::system::error_code& error, size_t bytes_transferred) {
		if (error) {
			std::cerr << "sync_head3_handler():" << boost::system::system_error(error).what() << std::endl;
			//socket_->close();//关闭连接
			return;
		}
		//std::cout<<"收到数据["<<bytes_transferred<<"]-->"<<std::endl;

		sync_head3_type_ = sync_head3_[0];

		sync_head3_length_ = ((uint8_t)sync_head3_[1]) << 8 | ((uint8_t)sync_head3_[2]);

		if (sync_head3_type_ == 1) {
			//std::cout<<"1"<<std::endl;
		}
		else {

		}

		//std::cout<<"rtp_type:"<<std::to_string(sync_head3_type_)<<std::endl;
		//std::cout<<"rtp_length:"<<std::to_string(sync_head3_length_)<<std::endl;


		//std::cout<<"start sync_body"<<std::endl;
		sync_body();

	}

	char sync_body_[10 * 1024];//包体 10kb缓存

	void sync_body() {

		//if (stop_) {
		//	std::cout << "执行 sync_body() 操作时检测到 stop_ 信号,不执行" << std::endl;
		//	teardown();//结束当前会话
		//	return;
		//}
		client_->read_async(sync_body_, sync_head3_length_, boost::bind(&rtsp::sync_body_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

		//std::cout<<"sync_body()"<<std::endl;
		//boost::asio::async_read(socket_, boost::asio::buffer(sync_body_, sync_head3_length_), boost::bind(&rtsp::sync_body_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}


	typedef struct {
		uint8_t v : 2;//版本号 2bit 当前版本号为2
		uint8_t p : 1;//填充符 1bit 如果p=1 则在该报文的尾部填充一个或多个额外的八位组，它不是有效载荷的一部分
		uint8_t x : 1;//拓展标志 1bit 如果x=1 则在rtp报tou后面跟有一个拓展报头
		uint8_t cc : 4;//csrc 计数器 4bit 指示csrc标志符的个数（作用信源csrc计数器）
		uint8_t m : 1;//标记 1bit 不同的在和数据有不同的含义，对于视频，标记一帧的结束;对于音频标记会话的开始。（对于分组的重要事件可以使用该标识）
		uint8_t pt : 7;//有效载荷类型 7bit 用于说明rtp报文中的有效载荷类型，如gsm视频/gpem图像等，在流媒体中大部分是用来区分音频流和视频流的，这样便于客户端解析。
		uint16_t sn : 16;//序列号 16bit，用于标记发送者所发送的rtp报文的序列号

		uint32_t timestamp : 32;//时间戳 32bit 必须使用90khz的时钟频率。时间戳反映了该rtp报文的第一个八位组的采样时刻。接收者使用时间戳来计算延迟和抖动，并进行同步控制

		uint32_t ssrc : 32;//同步信源ssrc标志符 32bit 用于标志同步信源。该标志是随机选择的，参加统一会议的两个同步信源不能有相同的ssrc

		uint32_t csrc[15];//特约信源csrc标志符 每个csrc 32bit 可以有0～15个 每个csrc标志了包含在该rtp报文有效载荷中的所有特约信源


	}rtp_head_type;



	typedef struct {

		union {
			struct {

				uint8_t type : 5;

				uint8_t nri : 2;

				uint8_t f : 1;
				//type
				/*
				 0: 没有定义
				 1-23:  单个nal单元包
				 24:    STAP-A 单一时间的组合包
				 25:    STAP-B 单一时间的组合包
				 26:    MTAP16 多个时间的组合包
				 27:    MTAP24 多个时间的组合包
				 28:    FU-A    分片单元
				 28:    FU-B    分片单元
				 30-31: 没有定义
				 */
			}instance;
			uint8_t fu;
		}indicator;


		union {
			struct {

				uint8_t type : 5;

				uint8_t r : 1;

				uint8_t e : 1;

				uint8_t s : 1;
			}instance;
			uint8_t fu;
		}header;

	}rtp_fu_type;




	rtp_head_type rtp_head_;
	rtp_fu_type rtp_fu_;



	void sync_body_handler(const boost::system::error_code& error, size_t bytes_transferred) {
		if (error) {
			std::cerr << "sync_body_handler():" << boost::system::system_error(error).what() << std::endl;
			return;
		}
		/*read_byte_sum_ += bytes_transferred;*/
		//std::cout<<"收到数据["<<bytes_transferred<<"]-->"<<std::endl;
		//std::cout<<"sync_body_handler()"<<std::endl;

		//首先提取必定存在的12byte数据
		rtp_head_.v = sync_body_[0] >> 6 & 0x3u;
		rtp_head_.p = sync_body_[0] >> 5 & 0x1u;
		rtp_head_.x = sync_body_[0] >> 4 & 0x1u;
		rtp_head_.cc = sync_body_[0] >> 0 & 0xfu;

		rtp_head_.m = sync_body_[1] >> 7 & 0x1u;
		rtp_head_.pt = sync_body_[1] >> 0 & 0x7fu;

		rtp_head_.sn = sync_body_[2] << 8 | sync_body_[3];

		rtp_head_.timestamp = sync_body_[4] << 24 | sync_body_[5] << 16 | sync_body_[6] << 8 | sync_body_[7];

		rtp_head_.ssrc = sync_body_[8] << 24 | sync_body_[9] << 16 | sync_body_[10] << 8 | sync_body_[11];

		for (int i = 0; i < rtp_head_.cc; ++i) {
			rtp_head_.csrc[i] = sync_body_[12 + i * 4 + 0] << 24 | sync_body_[12 + i * 4 + 1] << 16 | sync_body_[12 + i * 4 + 2] << 8 | sync_body_[12 + i * 4 + 3];
		}


		if (sync_head3_type_ == 1) {


			//std::cout<<"控制通道 丢弃！"<<std::endl;
			//            std::cout<<"版本："<<std::to_string(rtp_head_.v)<<std::endl;
			//            std::cout<<"填充符："<<std::to_string(rtp_head_.p)<<std::endl;
			//            std::cout<<"拓展标志："<<std::to_string(rtp_head_.x)<<std::endl;
			//            std::cout<<"参与源数："<<std::to_string(rtp_head_.cc)<<std::endl;
			//            std::cout<<"标记："<<std::to_string(rtp_head_.m)<<std::endl;
			//            std::cout<<"有效载荷类型："<<std::to_string(rtp_head_.pt)<<std::endl;
			//            std::cout<<"序列号："<<std::to_string(rtp_head_.sn)<<std::endl;
			//            std::cout<<"时间戳："<<std::to_string(rtp_head_.timestamp)<<std::endl;
			//            std::cout<<"同步信源标志符："<<std::to_string(rtp_head_.ssrc)<<std::endl;
			//
			//
			//            for(int i=0;i<rtp_head_.cc;++i){
			//                std::cout<<"特约信源标志符["<<i<<"]："<<std::to_string(rtp_head_.ssrc)<<std::endl;
			//            }


		}
		else {

			//判断是 单个NAL单元包 还是 分片单元（fu-a）

			rtp_fu_.indicator.fu = sync_body_[12 + rtp_head_.cc * 4];


			//std::cout<<"flag = "<<std::to_string(rtp_fu_.header.instance.type)<<std::endl;

			//std::cout<<"fu包值:"<<std::to_string(sync_body_[12 + rtp_head_.cc*4])<<std::endl;

			if (rtp_fu_.indicator.instance.type >= 1 && rtp_fu_.indicator.instance.type <= 23) {
				//1-23:  单个nal单元包
				//std::cout<<"单个nal单元包"<<std::endl;

				uint32_t sync_headx_32b = sync_body_[12 + rtp_head_.cc * 4] << 24 | sync_body_[12 + rtp_head_.cc * 4 + 1] << 16 | sync_body_[12 + rtp_head_.cc * 4 + 2] << 8 | sync_body_[12 + rtp_head_.cc * 4 + 3];

				if (sync_headx_32b == 0x67640028 || sync_headx_32b == 0x67428028) {
					std::cout << "同步头" << std::endl;
					/*if (index_ == 1)
					{
						recv_cnt1_++;
					}
					if (index_ == 2)
					{
						recv_cnt2_++;
					}*/
					// std::cout<<"recv_cnt1 = "<<std::to_string(recv_cnt1_)<<std::endl;
					// std::cout<<"recv_cnt2 = "<<std::to_string(recv_cnt2_)<<std::endl;
				   //std::cout<<"to_file_count = "<<std::to_string(to_file_count)<<std::endl;

					/*if (to_file_count >= 1024 * 1024 * 1024) {
						std::cout << "文件大小超过2M,重新创建文件" << std::endl;
						to_file_count = 0;
						fs_->close();
						fs2_->close();
						if (index_ = 1)
						{
							saver_dir_ = "/var/ftp/channel1";
							sd_dir_ = "/mnt/sdcard/channel1";

						}
						if (index_ = 1)
						{
							saver_dir_ = "/var/ftp/channel2";
							sd_dir_ = "/mnt/sdcard/channel2";

						}
						saver saver_(saver_dir_);

						uint32_t index_ = saver_.write();

						boost::filesystem::path path_(saver_dir_ + "/" + std::to_string(index_) + ".h264");
						boost::filesystem::path sd_path_(sd_dir_ + "/" + std::to_string(index_) + ".h264");

						std::cout << "sd_path_ = " << sd_path_ << std::endl;

						fs_ = new boost::filesystem::fstream(path_, std::ios::out);
						fs2_ = new boost::filesystem::fstream(sd_path_, std::ios::out);

					}*/
					//if (realtime_buffer_current_ != -1) {
					//	//上次正在操作了一片缓存片,现在已经处理成功
					//	realtime_buffer_busy_->push_back(realtime_buffer_current_);//压入发送队列

					//	float percent = realtime_buffer_pos_[realtime_buffer_current_] / (sizeof(realtime_buffer_) / 10.0f) * 100.0f;
					//	//std::cout<<"上次缓存片统计:{写入量,总量,占用百分比}={"<<std::to_string(realtime_buffer_pos_[realtime_buffer_current_])<<","<<std::to_string(sizeof(realtime_buffer_)/10)<<","<<std::to_string(percent)<<"}"<<std::endl;
					//	//std::cout<<"ip = "<<ip_<<" realtime_buffer_busy_.size()="<<realtime_buffer_busy_->size()<<std::endl;

					//	realtime_buffer_current_ = -1;
					//}

					////检测下传缓存的空位
					//if (!realtime_buffer_free_->empty()) {
					//	realtime_buffer_current_ = realtime_buffer_free_->at(0);//取出一个缓存编号
					//	realtime_buffer_free_->pop_front();
					//	realtime_buffer_pos_[realtime_buffer_current_] = 0;//清除游标
					//	//std::cout<<"获取到空闲的缓存片,序号:"<<realtime_buffer_current_<<std::endl;
					//}
					//else {
					//	//std::cout<<"当前没有空闲的缓存片"<<std::endl;
					//	realtime_buffer_current_ = -1;//没有被选中的缓存片
					//}
				}
				//char buffer[4] = { 0x00,0x00,0x00,0x01 };
				//fs_->write(buffer, 4);
				//fs2_->write(buffer, 4);

				//fs_->write((char*)& sync_body_[12 + rtp_head_.cc * 4], bytes_transferred - (12 + rtp_head_.cc * 4));
				//fs2_->write((char*)& sync_body_[12 + rtp_head_.cc * 4], bytes_transferred - (12 + rtp_head_.cc * 4));
				//fs_->flush();//刷新到物理储存器中
				//fs2_->flush();
				//to_file_count += 4 + bytes_transferred - (12 + rtp_head_.cc * 4);

				//if (realtime_buffer_current_ != -1) {
				//	//需要写入空闲缓存片
				//	memcpy(&realtime_buffer_[realtime_buffer_current_][realtime_buffer_pos_[realtime_buffer_current_]], buffer, 4);
				//	realtime_buffer_pos_[realtime_buffer_current_] += 4;
				//	memcpy(&realtime_buffer_[realtime_buffer_current_][realtime_buffer_pos_[realtime_buffer_current_]], (char*)& sync_body_[12 + rtp_head_.cc * 4], bytes_transferred - (12 + rtp_head_.cc * 4));
				//	realtime_buffer_pos_[realtime_buffer_current_] += bytes_transferred - (12 + rtp_head_.cc * 4);
				//}


			}
			else if (rtp_fu_.indicator.instance.type == 24) {
				//24:    STAP-A 单一时间的组合包
				std::cout << "STAP-A 单一时间的组合包" << std::endl;

			}
			else if (rtp_fu_.indicator.instance.type == 25) {
				//25:    STAP-B 单一时间的组合包
				std::cout << "STAP-B 单一时间的组合包" << std::endl;

			}
			else if (rtp_fu_.indicator.instance.type == 26) {
				//26:    MTAP16 多个时间的组合包
				std::cout << "MTAP16 多个时间的组合包" << std::endl;

			}
			else if (rtp_fu_.indicator.instance.type == 27) {
				//27:    MTAP24 多个时间的组合包
				std::cout << "MTAP24 多个时间的组合包" << std::endl;

			}
			else if (rtp_fu_.indicator.instance.type == 28) {
				//28:    FU-A    分片单元
				//std::cout<<"FU-A 分片单元"<<std::endl;

				rtp_fu_.header.fu = sync_body_[12 + rtp_head_.cc * 4 + 1];

				if (rtp_fu_.header.instance.s) {
					//分包开始
					//std::cout<<"分片NAL开始------>"<<std::endl;


					char buffer[4] = { 0x00, 0x00, 0x00, 0x01 };
					//fs_->write(buffer, 4);
					//fs2_->write(buffer, 4);


					int nal = ((int)sync_body_[12 + rtp_head_.cc * 4 + 0] & 0xe0) |
						((int)sync_body_[12 + rtp_head_.cc * 4 + 1] & 0x1f);

					//fs_->write((char*)& nal, 1);
					//fs2_->write((char*)& nal, 1);

					//fs_->write((char*)& sync_body_[12 + rtp_head_.cc * 4 + 2],
					//	bytes_transferred - (12 + rtp_head_.cc * 4 + 2));
					//fs2_->write((char*)& sync_body_[12 + rtp_head_.cc * 4 + 2],
					//	bytes_transferred - (12 + rtp_head_.cc * 4 + 2));

					//to_file_count += 4 + 1 + bytes_transferred - (12 + rtp_head_.cc * 4 + 2);

					//fs_->flush();//刷新到物理储存器中
					//fs2_->flush();


					//if (realtime_buffer_current_ != -1) {
					//	//需要写入空闲缓存片
					//	memcpy(&realtime_buffer_[realtime_buffer_current_][realtime_buffer_pos_[realtime_buffer_current_]], buffer, 4);
					//	realtime_buffer_pos_[realtime_buffer_current_] += 4;
					//	memcpy(&realtime_buffer_[realtime_buffer_current_][realtime_buffer_pos_[realtime_buffer_current_]], (char*)& nal, 1);
					//	realtime_buffer_pos_[realtime_buffer_current_] += 1;
					//	memcpy(&realtime_buffer_[realtime_buffer_current_][realtime_buffer_pos_[realtime_buffer_current_]], (char*)& sync_body_[12 + rtp_head_.cc * 4 + 2], bytes_transferred - (12 + rtp_head_.cc * 4 + 2));
					//	realtime_buffer_pos_[realtime_buffer_current_] += bytes_transferred - (12 + rtp_head_.cc * 4 + 2);
					//}


				}
				else if (rtp_fu_.header.instance.e) {
					//分包结束
					//std::cout<<"分片NAL结束<------"<<std::endl;


					//fs_->write((char*)& sync_body_[12 + rtp_head_.cc * 4 + 2],
					//	bytes_transferred - (12 + rtp_head_.cc * 4 + 2));
					//fs2_->write((char*)& sync_body_[12 + rtp_head_.cc * 4 + 2],
					//	bytes_transferred - (12 + rtp_head_.cc * 4 + 2));
					//to_file_count += 4 + 1 + bytes_transferred - (12 + rtp_head_.cc * 4 + 2);
					//fs_->flush();//刷新到物理储存器中
					//fs2_->flush();
					//if (realtime_buffer_current_ != -1) {
					//	//需要写入空闲缓存片
					//	memcpy(&realtime_buffer_[realtime_buffer_current_][realtime_buffer_pos_[realtime_buffer_current_]], (char*)& sync_body_[12 + rtp_head_.cc * 4 + 2], bytes_transferred - (12 + rtp_head_.cc * 4 + 2));
					//	realtime_buffer_pos_[realtime_buffer_current_] += bytes_transferred - (12 + rtp_head_.cc * 4 + 2);
					//}

				}
				else {
					//中间分包
					//std::cout<<"中间包<--->"<<std::endl;

					//fs_->write((char*)& sync_body_[12 + rtp_head_.cc * 4 + 2],
					//	bytes_transferred - (12 + rtp_head_.cc * 4 + 2));
					//fs2_->write((char*)& sync_body_[12 + rtp_head_.cc * 4 + 2],
					//	bytes_transferred - (12 + rtp_head_.cc * 4 + 2));
					//to_file_count += 4 + 1 + bytes_transferred - (12 + rtp_head_.cc * 4 + 2);

					//fs_->flush();//刷新到物理储存器中
					//fs2_->flush();

					//if (realtime_buffer_current_ != -1) {
					//	//需要写入空闲缓存片
					//	memcpy(&realtime_buffer_[realtime_buffer_current_][realtime_buffer_pos_[realtime_buffer_current_]], (char*)& sync_body_[12 + rtp_head_.cc * 4 + 2], bytes_transferred - (12 + rtp_head_.cc * 4 + 2));
					//	realtime_buffer_pos_[realtime_buffer_current_] += bytes_transferred - (12 + rtp_head_.cc * 4 + 2);
					//}
				}

			}
			else if (rtp_fu_.indicator.instance.type == 29) {
				//28:    FU-B    分片单元
				std::cout << "FU-B 分片单元" << std::endl;

			}
			else {
				//0 / 30-31: 没有定义
				std::cout << "没有定义" << std::endl;

			}
		}

		sync_head();
	}




};













int main()
{
    std::cout << "Hello World!\n"; 


	boost::asio::io_service service_;

	//tcpclient client("client", service_);

	////client.connect_sync("192.168.2.164", 8554);

	//client.connect_async("192.168.2.164", 8554, 
	//	[&](boost::system::error_code error_code) {
	//		if (error_code) {
	//			std::cout << "[ client ] --->. " << "Connection Failed:" << error_code.message() << std::endl;

	//			client.close();
	//		}
	//		std::cout << "[ client ] --->. " << "Connection Suuceed!" << std::endl;
	//	});

	rtsp rtsp("rtsp", service_);

	rtsp.start("192.168.2.164", 8554);

	service_.run();











}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门提示: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
