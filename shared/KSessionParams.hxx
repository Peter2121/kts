#pragma once
/*==============================================================================
 * params
 *=============================================================================*/
class KSessionParams
{
public:

	std::string rsakey_file;
	std::string active_sessions_dir;
	std::string subsystems_dir;

	std::string welcome_message;
	std::string login_message;
	std::string pass_message;
	std::string pass_timeout_message;
	std::string idle_timeout_message;
	std::string service_stopped_message;
	std::string login_successfull_message;
	std::string shell_dead_message;

	//		std::string allowed_login_list;
	//		std::string fordbiden_login_list;

	std::string shell_command;

	std::string default_user;
	std::string default_pass;
	std::string default_domain;

	std::string auto_logon_init;
	std::string publickey_logon_init;

	std::string log_file;
	std::string trace_file;
	std::string error_file;

	std::string key_init;
	std::string telnet_init;

	std::string sftp_root;
	std::string sftp_init;

	SHORT screen_width;
	SHORT screen_height;
	SHORT buff_height;

	unsigned first_packet_timeout;
	unsigned health_monitor_timeout;
	unsigned io_timeout;
	unsigned net_check_delay;
	unsigned pass_timeout;
	unsigned idle_timeout;
	unsigned refresh_delay;
	unsigned max_login_attempts;
	unsigned export_code_page;
	unsigned trace_level;

	int max_portforward_channels;

	bool dumb_client;
	bool debug_flag;
	bool allow_disconnected_sessions;
	bool auto_reconnect_session;
	//		bool allow_sftp;
	//		bool allow_port_forwarding;
	bool pipe_mode;


	std::string client_port;
	std::string client_ip;
	std::string user;

	std::string kex_algo_list;
	std::string encr_algo_list;
	std::string mac_algo_list;
	KSessionParams() : KSessionParams(KTS_INI_FILE) {}

	KSessionParams(std::string inifile)
	{
		KIni ini;
		ini.File(inifile);

		ini.GetKey("KSession", "rsakey_file", this->rsakey_file);
		ini.GetKey("KSession", "active_sessions_dir", this->active_sessions_dir);
		ini.GetKey("KSession", "subsystems_dir", this->subsystems_dir);

		ini.GetKey("KSession", "welcome_message", this->welcome_message);
		ini.GetKey("KSession", "pass_message", this->pass_message);
		ini.GetKey("KSession", "login_message", this->login_message);
		ini.GetKey("KSession", "pass_timeout_message", this->pass_timeout_message);
		ini.GetKey("KSession", "idle_timeout_message", this->idle_timeout_message);
		ini.GetKey("KSession", "service_stopped_message", this->service_stopped_message);
		ini.GetKey("KSession", "login_successfull_message", this->login_successfull_message);
		ini.GetKey("KSession", "shell_dead_message", this->shell_dead_message);


		ini.GetKey("KSession", "first_packet_timeout", this->first_packet_timeout);

		ini.GetKey("KSession", "health_monitor_timeout", this->health_monitor_timeout);
		ini.GetKey("KSession", "io_timeout", this->io_timeout);
		ini.GetKey("KSession", "net_check_delay", this->net_check_delay);

		ini.GetKey("KSession", "max_login_attempts", this->max_login_attempts);

		ini.GetKey("KSession", "shell_command", this->shell_command);


		ini.GetKey("KSession", "default_user", this->default_user);
		ini.GetKey("KSession", "default_pass", this->default_pass);
		ini.GetKey("KSession", "default_domain", this->default_domain);

		ini.GetKey("KSession", "auto_logon_init", this->auto_logon_init);
		ini.GetKey("KSession", "publickey_logon_init", this->publickey_logon_init);

		ini.GetKey("KSession", "trace_level", this->trace_level);
		ini.GetKey("KSession", "log_file", this->log_file);
		ini.GetKey("KSession", "trace_file", this->trace_file);
		ini.GetKey("KSession", "error_file", this->error_file);

		ini.GetKey("KSession", "key_init", this->key_init);
		ini.GetKey("KSession", "telnet_init", this->telnet_init);

		ini.GetKey("KSession", "pass_timeout", this->pass_timeout);
		ini.GetKey("KSession", "idle_timeout", this->idle_timeout);

		ini.GetKey("KSession", "refresh_delay", this->refresh_delay);

		ini.GetKey("KSession", "screen_width", this->screen_width);
		ini.GetKey("KSession", "screen_height", this->screen_height);
		ini.GetKey("KSession", "buff_height", this->buff_height);

		ini.GetKey("KSession", "dumb_client", this->dumb_client);
		ini.GetKey("KSession", "export_code_page", this->export_code_page);

		ini.GetKey("KSession", "allow_disconnected_sessions", this->allow_disconnected_sessions);
		ini.GetKey("KSession", "auto_reconnect_session", this->auto_reconnect_session);

		ini.GetKey("KSession", "sftp_init", this->sftp_init);
		ini.GetKey("KSession", "sftp_root", this->sftp_root);
		//			ini.GetKey( "KSession", "allow_sftp", this->allow_sftp );

		//			ini.GetKey( "KSession", "allow_port_forwarding", this->allow_port_forwarding );

		ini.GetKey("KSession", "pipe_mode", this->pipe_mode);

		ini.GetKey("KSession", "kex_algo_list", this->kex_algo_list);
		ini.GetKey("KSession", "encr_algo_list", this->encr_algo_list);
		ini.GetKey("KSession", "mac_algo_list", this->mac_algo_list);
		ini.GetKey("KSession", "max_portforward_channels", this->max_portforward_channels);

		this->client_port = KWinsta::GetCmdLineParam("-port:");
		this->client_ip = KWinsta::GetCmdLineParam("-ip:");

		SetEnvironmentVariable("KTS_VERSION", KTS_VERSION);
		SetEnvironmentVariable("KTS_IP", this->client_ip.c_str());
		SetEnvironmentVariable("KTS_PORT", this->client_port.c_str());

		KWinsta::ExpandEnvironmentString(this->welcome_message);
		KWinsta::ExpandEnvironmentString(this->pass_message);
		KWinsta::ExpandEnvironmentString(this->pass_timeout_message);
		KWinsta::ExpandEnvironmentString(this->idle_timeout_message);
		KWinsta::ExpandEnvironmentString(this->shell_command);

		KWinsta::ExpandEnvironmentString(this->log_file);
		KWinsta::ExpandEnvironmentString(this->trace_file);
		KWinsta::ExpandEnvironmentString(this->error_file);
		KWinsta::ExpandEnvironmentString(this->rsakey_file);
		KWinsta::ExpandEnvironmentString(this->key_init);
		KWinsta::ExpandEnvironmentString(this->telnet_init);
		KWinsta::ExpandEnvironmentString(this->active_sessions_dir);
		KWinsta::ExpandEnvironmentString(this->subsystems_dir);


		KWinsta::ExpandEnvironmentString(this->auto_logon_init);
		KWinsta::ExpandEnvironmentString(this->publickey_logon_init);

	}
};