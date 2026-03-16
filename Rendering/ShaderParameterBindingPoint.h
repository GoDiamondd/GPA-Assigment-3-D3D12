#pragma once

namespace INANOA {
	namespace SHADER_PARAMETER_BINDING {

		// Input Assembler Slot (matches InputLayout "POSITION" slot 0)
		const unsigned int VERTEX_LOCATION = 0;

		// Root Signature Slots (Must match RendererBase::init order)
		const unsigned int VIEW_MAT_LOCATION = 0;   // Root Parameter 0
		const unsigned int PROJ_MAT_LOCATION = 1;   // Root Parameter 1
		const unsigned int MODEL_MAT_LOCATION = 2;  // Root Parameter 2
		const unsigned int SHADING_MODEL_ID_LOCATION = 3; // Root Parameter 3
	}
}