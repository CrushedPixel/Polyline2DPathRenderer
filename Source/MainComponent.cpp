#include "MainComponent.h"

using GL = juce::OpenGLExtensionFunctions;

MainComponent::MainComponent() :
		randomSeed(juce::Time::currentTimeMillis()) {

	// we use modern OpenGL, because we're classy
	openGLContext.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
	openGLContext.setMultisamplingEnabled(true);

	path.startNewSubPath(-0.7f, -0.7f);
	path.lineTo(-0.3f, -0.7f);
	path.lineTo(-0.4f, -0.4f);
	path.lineTo(-0.2f, -0.4f);
	path.closeSubPath();

	path.startNewSubPath(0.5f, 0.5f);
	path.quadraticTo(0.25f, 0.0f,
	                 0.0f, 0.0f);

	path.addStar({-0.5f, 0.5f}, 8, 0.2f, 0.3f);

	path.addEllipse(0.3f, -0.75f, 0.4f, 0.6f);

	setSize(800, 800);
}

MainComponent::~MainComponent() {
	// This shuts down the GL system and stops the rendering calls.
	shutdownOpenGL();
}

void MainComponent::initialise() {
	// generate VAO
	GL::glGenVertexArrays(1, &vaoHandle);
	GL::glBindVertexArray(vaoHandle);

	// generate VBOs
	GL::glGenBuffers(1, &vboHandle);
	GL::glBindBuffer(GL_ARRAY_BUFFER, vboHandle);
	vboSize = 0;

	// generate Shader Program
	auto vert = GL::glCreateShader(GL_VERTEX_SHADER);
	auto frag = GL::glCreateShader(GL_FRAGMENT_SHADER);

	GL::glShaderSource(vert, 1, &BinaryData::Passthrough_vert, &BinaryData::Passthrough_vertSize);
	GL::glShaderSource(frag, 1, &BinaryData::PlainColor_frag, &BinaryData::PlainColor_fragSize);

	GL::glCompileShader(vert);
	GL::glCompileShader(frag);
	// no error checking, I don't make mistakes (lol)

	programHandle = GL::glCreateProgram();
	GL::glAttachShader(programHandle, vert);
	GL::glAttachShader(programHandle, frag);
	GL::glLinkProgram(programHandle);

	GL::glDeleteShader(vert);
	GL::glDeleteShader(frag);

	// get shader attribute locations
	posInAttribLocation = (GLuint) GL::glGetAttribLocation(programHandle, "posIn");
	colorUniformLocation = (GLuint) GL::glGetUniformLocation(programHandle, "color");

	// the posIn is filled with tightly packed Vec2s (which are just 2 floats in memory)
	GL::glVertexAttribPointer(posInAttribLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	GL::glEnableVertexAttribArray(posInAttribLocation);
}

void MainComponent::shutdown() {
	GL::glDeleteVertexArrays(1, &vaoHandle);
	GL::glDeleteBuffers(1, &vboHandle);
	GL::glDeleteProgram(programHandle);
}

static void writeVec2(juce::MemoryOutputStream &out, const crushedpixel::Vec2 &vec) {
	// write vertices as two consecutive floats
	out.writeFloat(vec.x);
	out.writeFloat(vec.y);
}

void MainComponent::render() {
	// transform the path into a mesh
	juce::PathFlatteningIterator it(path, juce::AffineTransform(),
			// since we're in the OpenGL coordinate system,
			// the default precision is not enough
			0.001f);

	std::vector<crushedpixel::Vec2> points;
	std::vector<crushedpixel::Vec2> vertices;
	std::vector<size_t> subPathVertexCounts;

	bool subpathClosed = false;
	while (true) {
		bool hasNext = it.next();
		if (!hasNext || it.subPathIndex == 0) {
			// a new subpath was begun -
			// generate vertices for the previous subpath

			if (!points.empty()) {
				constexpr auto thickness = 0.03f;

				auto endCapStyle = crushedpixel::Polyline2D::EndCapStyle::ROUND;
				if (subpathClosed) {
					endCapStyle = crushedpixel::Polyline2D::EndCapStyle::JOINED;
				}

				auto numVertices = crushedpixel::Polyline2D::create(vertices, points, thickness,
				                                                    crushedpixel::Polyline2D::JointStyle::ROUND,
				                                                    endCapStyle);
				subPathVertexCounts.push_back(numVertices);
			}

			if (!hasNext) {
				// this was the last subpath
				break;
			}

			points.clear();
		}

		// add the line segment's start point to the set of points
		points.emplace_back(it.x1, it.y1);

		if (it.isLastInSubpath() && !it.closesSubPath) {
			// only add the last point if it doesn't close the subpath,
			// as Polyline2D takes care of connecting the start and end points itself.
			points.emplace_back(it.x2, it.y2);
		}

		subpathClosed = it.closesSubPath;
	}

	// convert the vertex data into bytes
	juce::MemoryOutputStream mos;
	for (auto &vertex : vertices) {
		writeVec2(mos, vertex);
	}

	// write the vertices into the buffer, expanding it if necessary
	GL::glBindBuffer(GL_ARRAY_BUFFER, vboHandle);

	auto newSize = (GLsizei) mos.getDataSize();
	if (newSize > vboSize) {
		GL::glBufferData(GL_ARRAY_BUFFER, newSize, mos.getData(), GL_DYNAMIC_DRAW);
		vboSize = newSize;
	} else {
		GL::glBufferSubData(GL_ARRAY_BUFFER, 0, newSize, mos.getData());
	}

	/*
	 * RENDERING
	 */

	// fill the background with an ever-changing color
	constexpr auto hueSpeed = 5 * 1000;
	OpenGLHelpers::clear(juce::Colour::fromHSV(
			Time::currentTimeMillis() % hueSpeed / (float) hueSpeed,
			0.3f, 0.2f, 1));

	// render the subpaths
	GL::glBindVertexArray(vaoHandle);
	GL::glUseProgram(programHandle);

	// reset the random instance to get consistent colors
	// over multiple render frames
	random.setSeed(randomSeed);

	size_t vertexOffset = 0;
	for (auto numVertices : subPathVertexCounts) {
		// set color for subpath
		auto color = juce::Colour::fromHSV(
				random.nextInt(255) / 255.0f,
				random.nextInt({150, 255}) / 255.0f,
				random.nextInt({170, 255}) / 255.0f,
				1);

		GL::glUniform4f(colorUniformLocation,
		                color.getFloatRed(),
		                color.getFloatGreen(),
		                color.getFloatBlue(),
		                color.getFloatAlpha());

		glDrawArrays(GL_TRIANGLES, (GLint) vertexOffset, (GLint) numVertices);
		vertexOffset += numVertices;
	}
}

void MainComponent::paint(Graphics &g) {
	// You can add your component specific drawing code here!
	// This will draw over the top of the openGL background.
}

void MainComponent::resized() {
	// This is called when the MainComponent is resized.
	// If you add any child components, this is where you should
	// update their positions.
}
