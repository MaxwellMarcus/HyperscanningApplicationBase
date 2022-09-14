#include "HyperscanningTest.h"
#include "Shapes.h"
#include <limits>
#include <SDL2/SDL.h>
#include "BCIEvent.h"

RegisterFilter( HyperscanningTest, 3 );

HyperscanningTest::HyperscanningTest() : mpRect( NULL ), mrDisplay( Window() ) {
}

void HyperscanningTest::Publish() {
}

void HyperscanningTest::Preflight( const SignalProperties& Input, SignalProperties& Output ) const {
	if ( State( "Color" )->Length() != 1 ) {
		bciwarn << State( "Color" );
		bcierr << "The color state must be defined";
	}
}

void HyperscanningTest::Initialize( const SignalProperties& Input, const SignalProperties& Output ) {
	ApplicationBase::Initialize( Input, Output );

	GUI::Rect rect = { 0.1f, 0.4f, 0.9f, 0.6f };

	mpRect = new RectangularShape( mrDisplay );
	mpRect->SetFillColor( RGBColor::Teal );
	mpRect->SetObjectRect( rect );
	mpRect->SetVisible( true );
}

void HyperscanningTest::Process( const GenericSignal& Input, GenericSignal& Output ) {
	mpRect->SetFillColor( State( "Color" ) ? RGBColor::Teal : RGBColor::Red );
	while( SDL_PollEvent( &e ) != 0 ) {
		if ( e.type == SDL_MOUSEBUTTONDOWN ) {
			bciwarn << "Original: " << State( "Color" );
			State( "Color" ) = !State( "Color" );
			bciwarn << "Updated: " << State( "Color" );
		}
	}
}
