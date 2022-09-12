#include "HyperscanningNetworkLogger.h"
#include "BCIStream.h"
#include "Thread.h"
#include "BCIEvent.h"

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <mutex>
#include <sstream>
#include <iostream>

Extension( HyperscanningNetworkLogger );

HyperscanningNetworkLogger::HyperscanningNetworkLogger() : mLogNetwork( false ) {
}

HyperscanningNetworkLogger::~HyperscanningNetworkLogger() {
	mTerminate = true;
	Halt();
}

void HyperscanningNetworkLogger::Publish() {
	if ( OptionalParameter( "LogNetwork" ) > 0 ) {
		BEGIN_PARAMETER_DEFINITIONS
			"Source:Hyperscanning%20Network%20Logger int LogNetwork= 1 0 0 1"
			" // record hyperscanning network states (boolean) ",
			"Source:Hyperscanning%20Network%20Logger stringmatrix SharedStates= { Name Size } 1 Color 1 % % % %"
			" // States to share with the server and other clients"
		END_PARAMETER_DEFINITIONS
		//bciwarn << OptionalParameter( "SharedStates" );
		std::string states( OptionalParameter( "SharedStates" ) );
		std::istringstream f( states );
		std::string name;
		std::string size;
		std::vector<std::string> sharedStates;
		int i = 0;
		while ( getline( f, name, ',' ) ) {
			if ( !getline( f, size, ';' ) )
				bcierr << "Every Shared State must have a size";
			bciwarn << "Shared State: " << name << ", " << size;
			BEGIN_EVENT_DEFINITIONS
				name + " " + size + " 0 0 0"
			END_EVENT_DEFINITIONS

			sharedStates.push_back( name );
		}
		mSharedStates = sharedStates;
		mPreviousStates = std::vector<uint32_t>( mSharedStates.size(), 0 );
	}
}
//
void HyperscanningNetworkLogger::Preflight() const {
	if ( Parameter( "SharedStates" )->NumValues() < 1 )// || Parameter( "SharedStates" )->NumValues() % 2 == 1 )
		bcierr << "You must have at least one shared state and a name and size for each";

}

void HyperscanningNetworkLogger::Initialize() {
	mLogNetwork = ( OptionalParameter( "LogNetwork" ) > 0 );

}

void HyperscanningNetworkLogger::StartRun() {
	if ( mLogNetwork ) {
		Setup();
		bciwarn << "Starting Network Thread";
		Start();
	}
}

void HyperscanningNetworkLogger::Process() {
	const std::lock_guard<std::mutex> lock( mMessageMutex );
	mMessage = "";

	for ( int i = 0; i < mSharedStates.size(); i++ ) {
		std::string message( mSharedStates[ i ] );
		StateRef s = State( mSharedStates[ i ] );
		uint32_t val = s;
		if ( val != mPreviousStates[ i ] ) {
			message.push_back( '\0' );
			message.push_back( ( char ) s->Length() );
			message += std::string( ( char* )( &val ), s->Length() ).c_str();

			mMessage += message;

			mPreviousStates[ i ] = val;
		}
	}
	mMessage.push_back( '\0' );
}
//
void HyperscanningNetworkLogger::StopRun() {
	bciwarn << "Stop Run";
}

void HyperscanningNetworkLogger::Halt() {
	StopRun();
}


void HyperscanningNetworkLogger::Setup() {
	mAddress = "10.138.1.182";
	//mAddress = "127.0.0.1";
	//mAddress = "172.20.10.10";
	bciout << "Connecting to socket";
	sockfd = socket( AF_INET, SOCK_STREAM, 0 );
	if ( sockfd == -1 )
		bcierr << "Failed to create socket. errno: " << errno;

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( mPort );

	if ( inet_pton( AF_INET, mAddress, &serv_addr.sin_addr ) <= 0 )
		bcierr << "Invalid address / address not supported";
	
	if ( ( clientfd = connect( sockfd, ( struct sockaddr* )&serv_addr, sizeof( serv_addr ) ) ) < 0 )
		bcierr << "Connection failed...";
}

int HyperscanningNetworkLogger::OnExecute() {

	bciout << "Awaiting server greeting...";
	buffer = ( char* )malloc( 1025 * sizeof( char ) );
	read( sockfd, buffer, 1025 );
	bciout << "Server Greeting: " << buffer;

	while ( !mTerminate ) {
		mMessageMutex.lock();
		std::string name( mMessage.c_str() );
		char size = *( mMessage.c_str() + name.size() + 1 );
		char value = *( mMessage.c_str() + name.size() + 2 );
		if ( send( sockfd, mMessage.c_str(), mMessage.size(), 0 ) < 0 )
			bciwarn << "Error writing to socket: " << errno;
		mMessageMutex.unlock();

		if ( read( sockfd, buffer, 1025 ) < 0 )
			bciwarn << "Error reading socket: " << errno;
		Interpret( buffer );
	}
	return 1;
}

void HyperscanningNetworkLogger::Interpret( char* buffer ) {
	char* cpy = buffer;
	while ( *buffer != '\0' ) {
		std::string name( buffer );
		buffer += name.size() + 1;
		char size = *buffer++;
		std::string value( buffer, size );
		buffer += size;

		char val = *value.c_str();

		bciwarn << name << ": " << val;

		bcievent << name << " " << val;
	}
}

