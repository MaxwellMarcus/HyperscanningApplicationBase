#ifndef NETWORK_LOGGER_H
#define NETWORK_LOGGER_H

#include "Environment.h"
#include "Thread.h"
#include "Sockets.h"
#include <atomic>
#include <mutex>

class HyperscanningNetworkThread : public Thread {
	public:
	private:
};

class HyperscanningNetworkLogger : public EnvironmentExtension, public Thread {
	public:
		HyperscanningNetworkLogger();
		~HyperscanningNetworkLogger();

		void Publish() override;
		void AutoConfig() override;
		void Preflight() const override;
		void Initialize() override;
		void StartRun() override;
		void Process() override;
		void StopRun() override;
		void Halt() override;

		int OnExecute() override;
		void Setup();

		void Interpret( char* );

	private:

		//int on_create() override;
//		int on_open() override {
//		    return 1;
//		}

		bool mLogNetwork;

		std::vector<std::string> mSharedStates;
		std::vector<uint32_t> mPreviousStates;
		std::vector<bool> mHasUpdated;
		std::mutex mPreviousStatesMutex;
		std::string mMessage;
		std::mutex mMessageMutex;

		ClientTCPSocket mSocket;
/*		int sockfd;
		int clientfd; */
		char* mBuffer;

		std::string mAddress;
		int mPort;

		char mClientNumber;
};

#endif
