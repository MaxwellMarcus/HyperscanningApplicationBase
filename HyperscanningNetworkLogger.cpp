#include "HyperscanningNetworkLogger.h"
#include "BCIStream.h"
#include "Thread.h"
#include "BCIEvent.h"
#include "sockstream.h"

#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <mutex>
#include <sstream>
#include <iostream>
#include <fstream>

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
			"Source:Hyperscanning%20Network%20Logger string IPAddress= 127.0.0.1 % % %"
			" // IPv4 address of server",
			"Source:Hyperscanning%20Network%20Logger int Port= 9999 % % %"
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
		mHasUpdated = std::vector<bool>( mSharedStates.size(), true );

		if ( OptionalParameter( "LogNetwork" ) > 0 ) {
			Setup();
			bciwarn << "Starting Network Thread";
			Start();
		}
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
	State( "ClientNumber" ) = ClientNumber;

}

void HyperscanningNetworkLogger::AutoConfig() {
	Parameters->Load( "DownloadedParameters.prm", false );
}

void HyperscanningNetworkLogger::StartRun() {
}

void HyperscanningNetworkLogger::Process() {
	if ( !mLogNetwork ) return;
	bciwarn << "Client Number: " << State( "ClientNumber" );

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
}

void HyperscanningNetworkLogger::Halt() {
	StopRun();
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
	open( mAddress, ( unsigned short )mPort );
	bciout << "Is open: " << is_open();
	bciout << "Address: " << address();
	bciout << "Connect: " << connect();
	bciout << "Connected: " << connected();

	bciout << "Awaiting server greeting...";

	std::string param_file;
	buffer = ( char* )malloc( 1025 * sizeof( char ) );
	read( buffer, 1025 );

	while ( buffer[ 1024 ] != 0 ) {
		param_file += std::string( buffer, 1025 );
		memset( buffer, 0, 1025 );
		bciwarn << "Size: " << param_file.size();
		read( buffer, 1025 );
	}

	param_file += std::string( buffer );

	std::ofstream outfile ( "DownloadedParameters.prm" );
	outfile << param_file;

	bciout << "Recieved server greeting...";

	if ( read( buffer, 1025 ) < 0 )
		bciwarn << "Error reading socket: " << errno;

	while ( *buffer != '\0' ) {
		std::string name( buffer );
		buffer += name.size() + 1;
		char size = *buffer++;
		std::string value( buffer, size );
		buffer += size;

		uint32_t val = *value.c_str();

		ClientNumber = val;
	}
}

int HyperscanningNetworkLogger::OnExecute() {
	while ( !mTerminate ) {
		memset( buffer, 0, 1025 );
		mMessageMutex.lock();
		std::string name( mMessage.c_str() );
		char size = *( mMessage.c_str() + name.size() + 1 );
		char value = *( mMessage.c_str() + name.size() + 2 );

		mMessage.push_back( '\0' );

		if ( send( mMessage.c_str(), mMessage.size(), 0 ) < 0 )
			bciwarn << "Error writing to socket: " << errno;
		mMessageMutex.unlock();

		if ( read( buffer, 1025 ) < 0 )
			bciwarn << "Error reading socket: " << errno;
		Interpret( buffer );
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

		uint32_t val = *value.c_str();

		bciwarn << name << ": " << ( int )val;
		mPreviousStatesMutex.lock();
		auto it = find( mSharedStates.begin(), mSharedStates.end(), name );
		mPreviousStates[ it - mSharedStates.begin() ] = val;
		mHasUpdated[ it - mSharedStates.begin() ] = false;
		mPreviousStatesMutex.unlock();

		bcievent << name << " " << val;
	}
}

