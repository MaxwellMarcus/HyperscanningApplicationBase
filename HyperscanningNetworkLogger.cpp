#include "HyperscanningNetworkLogger.h"
#include "BCIStream.h"
#include "Thread.h"
#include "BCIEvent.h"

#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <mutex>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>

#if _WIN32
#include <Winsock2.h>
#undef errno
#define errno WSAGetLastError()
#endif

Extension( HyperscanningNetworkLogger );

HyperscanningNetworkLogger::HyperscanningNetworkLogger()
: mBuffer(nullptr), mLogNetwork( false ) {
}

HyperscanningNetworkLogger::~HyperscanningNetworkLogger() {
	::free(mBuffer);
	Halt();
}

void HyperscanningNetworkLogger::Publish() {
	if ( OptionalParameter( "LogNetwork" ) > 0 ) {
		BEGIN_PARAMETER_DEFINITIONS
			"Source:Hyperscanning%20Network%20Logger int LogNetwork= 1 0 0 1"
			" // record hyperscanning network states (boolean) ",
			"Source:Hyperscanning%20Network%20Logger string IPAddress= 10.39.97.98 % % %"
			" // IPv4 address of server",
			"Source:Hyperscanning%20Network%20Logger int Port= 13774 % % %"
			" // server port"
		END_PARAMETER_DEFINITIONS

		BEGIN_EVENT_DEFINITIONS
			"ClientNumber 8 0 0 0"
		END_EVENT_DEFINITIONS

		std::string states( OptionalParameter( "SharedStates" ) );
		std::istringstream f( ( states ) );
		std::string name;
		std::string size;
		std::vector<std::string> sharedStates;
		int i = 0;
		while ( getline( f, name, ',' ) ) {
			if ( !getline( f, size, '&' ) )
				bcierr << "Every Shared State must have a size";
			bciwarn << "Shared State: " << name << ", " << size;
			BEGIN_EVENT_DEFINITIONS
				name + " " + size + " 0 0 0"
			END_EVENT_DEFINITIONS

			sharedStates.push_back( name );
		}
		mSharedStates = sharedStates;
		mPreviousStates = std::vector<uint32_t>( mSharedStates.size(), 0 );
		mHasUpdated = std::vector<bool>( mSharedStates.size(), true );
	}
}
//
void HyperscanningNetworkLogger::Preflight() const {
	if ( OptionalParameter( "LogNetwork" ) > 0 ) {
		if ( Parameter( "SharedStates" )->NumValues() < 1 )// || Parameter( "SharedStates" )->NumValues() % 2 == 1 )
			bcierr << "You must have at least one shared state and a name and size for each";
		if ( !OptionalParameter( "IPAddress" ) )
			bcierr << "Must give server address";
		if ( !OptionalParameter( "Port" ) )
			bcierr << "Must specify port";
	}

}

void HyperscanningNetworkLogger::Initialize() {
	mLogNetwork = ( OptionalParameter( "LogNetwork" ) > 0 );
	State( "ClientNumber" ) = mClientNumber;

}

void HyperscanningNetworkLogger::AutoConfig() {
	Parameters->Load( "DownloadedParameters.prm", false );
	if (OptionalParameter("LogNetwork") > 0)
		Setup();
}

void HyperscanningNetworkLogger::StartRun() {
	if (mLogNetwork) {
		bciwarn << "Starting Network Thread";
		Start();
	}
}

void HyperscanningNetworkLogger::Process() {
	if ( !mLogNetwork ) return;

	const std::lock_guard<std::mutex> lock( mMessageMutex );
	mMessage = "";

	for ( int i = 0; i < mSharedStates.size(); i++ ) {
		std::string message( mSharedStates[ i ] );
		StateRef s = State( mSharedStates[ i ] );
		uint32_t val = s;
		if ( val != mPreviousStates[ i ] ) {
			if ( mHasUpdated[ i ] ) {
				bciwarn << val << " != " << mPreviousStates[ i ];
				message.push_back( '\0' );
				message.push_back( ( char ) s->Length() );
				message += std::string( ( char* )( &val ), s->Length() ).c_str();

				mMessage += message;

				mPreviousStatesMutex.lock();
				mPreviousStates[ i ] = val;
				mPreviousStatesMutex.unlock();
			}
		} else
			mHasUpdated[ i ] = true;
	}
	mMessage.push_back( '\0' );
}
//
void HyperscanningNetworkLogger::StopRun() {
	bciwarn << "Stop Run";
	TerminateAndWait();
}

void HyperscanningNetworkLogger::Halt() {
	TerminateAndWait();
}

//int HyperscanningNetworkLogger::on_create() {
//	return m_fd = ::socket( AF_INET, SOCK_STREAM, 0 );
//}


void HyperscanningNetworkLogger::Setup() {
	//mAddress = "10.138.1.182";
	//mAddress = "127.0.0.1";
	//mAddress = "172.20.10.10";
	mAddress = ( std::string )OptionalParameter( "IPAddress" );
	mPort = OptionalParameter( "Port" );

	bciwarn << "Connecting to " << mAddress << ":" << ( unsigned short )mPort;
	
//	sockfd = socket( AF_INET, SOCK_STREAM, 0 );
//	if ( sockfd == -1 )
//		bcierr << "Failed to create socket. errno: " << errno;
//
//	serv_addr.sin_family = AF_INET;
//	serv_addr.sin_port = htons( mPort );
//
//	if ( inet_pton( AF_INET, mAddress.c_str(), &serv_addr.sin_addr ) <= 0 )
//		bcierr << "Invalid address / address not supported";
//	
//	if ( ( clientfd = connect( sockfd, ( struct sockaddr* )&serv_addr, sizeof( serv_addr ) ) ) < 0 )
//		bcierr << "Connection failed...";

	bciout << "Open: ";
	mSocket.Open( mAddress, ( unsigned short )mPort );
	bciout << "Is open: " << mSocket.IsOpen();
	bciout << "Address: " << mSocket.Address();
	bciout << "Connected: " << mSocket.Connected();

	bciout << "Awaiting server greeting...";

	std::string param_file;
	mBuffer = ( char* )malloc( 1025 * sizeof( char ) );
	::recv(mSocket.Fd(), mBuffer, 1025, 0); // read one packet only

	while ( mBuffer[ 1024 ] != 0 ) {
		param_file += std::string( mBuffer, 1025 );
		memset( mBuffer, 0, 1025 );
		bciwarn << "Size: " << param_file.size();
		::recv(mSocket.Fd(), mBuffer, 1025, 0); // read one packet only
	}

	param_file += std::string( mBuffer );

	std::ofstream outfile ( "DownloadedParameters.prm" );
	outfile << param_file;

	bciout << "Recieved server greeting...";

	if ( ::recv(mSocket.Fd(), mBuffer, 1025, 0 ) < 0 ) // read one packet only
		bciwarn << "Error reading socket: " << errno;

	char* buffer = mBuffer;
	while ( *buffer != '\0' ) {
		std::string name( buffer );
		buffer += name.size() + 1;
		char size = *buffer++;
		std::string value( buffer, size );
		buffer += size;

		uint32_t val;
		memcpy( &val, value.c_str(), size );

		mClientNumber = val;
	}
}

int HyperscanningNetworkLogger::OnExecute() {
	while ( !Terminating() ) {
		memset( mBuffer, 0, 1025 );

		{
			std::lock_guard<std::mutex> messageLock(mMessageMutex);
			std::string name(mMessage.c_str());
			char size = *(mMessage.c_str() + name.size() + 1);
			char value = *(mMessage.c_str() + name.size() + 2);

			mMessage.push_back('\0');

			if (mSocket.Write(mMessage.c_str(), mMessage.size()) < 0)
			{
				bciwarn << "Error writing to socket: " << errno;
				return -1;
			}
		}

		if (mSocket.Wait()) // will return false when thread is terminating
		{
			if (::recv(mSocket.Fd(), mBuffer, 1025, 0) < 0) // read one packet only
			{
				bciwarn << "Error reading socket: " << errno;
				return -1;
			}
			Interpret(mBuffer);
		}
	}
	return 1;
}

void HyperscanningNetworkLogger::Interpret( char* buffer ) {
	while ( *buffer != '\0' ) {
		std::string name( buffer );
		buffer += name.size() + 1;
		char size = *buffer++;
		std::string value( buffer, size );
		buffer += size;

		uint32_t val;
		memcpy( &val, value.c_str(), size );

		bciwarn << name << ": " << ( int )val;
		mPreviousStatesMutex.lock();
		auto it = find( mSharedStates.begin(), mSharedStates.end(), name );
		mPreviousStates[ it - mSharedStates.begin() ] = val;
		mHasUpdated[ it - mSharedStates.begin() ] = false;
		mPreviousStatesMutex.unlock();

		bcievent << name << " " << val;
	}
}

