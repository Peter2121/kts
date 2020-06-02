#pragma once
// =============================================================================
// params
// =============================================================================
class KDaemonParams
{
public:

	std::string service_name;
	std::string service_info;

	unsigned trace_level;
	std::string error_file;
	std::string trace_file;
	std::string log_file;

	std::string active_sessions_dir;

	int port;
	std::string ip;
	int max_sessions;
	bool debug_flag;
	bool use_ssh;

	KDaemonParams() : KDaemonParams(KTS_INI_FILE) {}

	KDaemonParams(std::string inifile)
	{
		KIni ini;
		ini.File(inifile);

		ini.GetKey("KDaemon", "service_name", this->service_name);
		ini.GetKey("KDaemon", "service_info", this->service_info);

		ini.GetKey("KDaemon", "trace_level", this->trace_level);
		ini.GetKey("KDaemon", "error_file", this->error_file);
		ini.GetKey("KDaemon", "trace_file", this->trace_file);
		ini.GetKey("KDaemon", "log_file", this->log_file);

		ini.GetKey("KDaemon", "port", this->port);
		ini.GetKey("KDaemon", "ip", this->ip);
		ini.GetKey("KDaemon", "max_sessions", this->max_sessions);

		ini.GetKey("KDaemon", "debug_flag", this->debug_flag);

		ini.GetKey("KDaemon", "use_ssh", this->use_ssh);

		ini.GetKey("KSession", "active_sessions_dir", this->active_sessions_dir);

		KWinsta::ExpandEnvironmentString(this->active_sessions_dir);
		KWinsta::ExpandEnvironmentString(this->log_file);
		KWinsta::ExpandEnvironmentString(this->trace_file);
		KWinsta::ExpandEnvironmentString(this->error_file);
	}
};