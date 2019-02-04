#pragma once

#include <JuceHeader.h>
#include <Polyline2D/Polyline2D.h>

class MainComponent : public OpenGLAppComponent {
public:
	MainComponent();

	~MainComponent();

	void initialise() override;

	void shutdown() override;

	void render() override;

	void paint(Graphics &g) override;

	void resized() override;

private:
	GLuint vaoHandle, vboHandle, programHandle;

	GLuint posInAttribLocation;
	GLuint colorUniformLocation;

	/**
	 * The current size of the VBO.
	 */
	GLsizei vboSize;

	/**
	 * The path to render using OpenGL.
	 */
	juce::Path path;

	/**
	 * Random instance with fixed seed,
	 * used to consistently generate subpath colors.
	 */
	juce::Random random;

	juce::int64 randomSeed;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
