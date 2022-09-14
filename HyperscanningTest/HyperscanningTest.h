#include "Shapes.h"
#include "ApplicationWindow.h"
#include "ApplicationBase.h"
#include <SDL2/SDL.h>

class HyperscanningTest : public ApplicationBase {
	public:
		HyperscanningTest();

		void Publish() override;

		void Preflight( const SignalProperties& Input, SignalProperties& Output ) const override;
		void Initialize( const SignalProperties& Input, const SignalProperties& Output ) override;
		void Process( const GenericSignal& Input, GenericSignal& Output ) override;

	private:
		ApplicationWindow& mrDisplay;
		RectangularShape* mpRect;
		SDL_Event e;
		int c;
};
