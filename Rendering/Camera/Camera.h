#pragma once

#include <glm\mat4x4.hpp>
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif

namespace INANOA {

	class Camera
	{
	public:
		explicit Camera();
		explicit Camera(const glm::vec3& viewOrg, const glm::vec3& lookCenter, const glm::vec3& upVector, const float distance, const float fovDeg, const float fNear, const float fFar);
		virtual ~Camera();

		// prohibit copy constructor
		Camera(const Camera&) = delete;
		// prohibit assignment
		Camera operator=(const Camera&) = delete;

	public:
		void reset(const glm::vec3& viewOrg, const glm::vec3& lookCenter, const glm::vec3& upVector, const float distance);
		
		void update();

		void setViewOrigBasedOnLookCenter(const glm::vec3& tVec);
		void translateLookCenterAndViewOrg(const glm::vec3& t);

		void forward(const glm::vec3& forwardMagnitude, const bool disableYDimension);
		void rotateLookCenterAccordingToViewOrg(const float rad);

		void resize(const int w, const int h);

	public:
		inline void setViewOrg(const glm::vec3& org) {
			this->m_viewOrg = org;
		}
		inline void setLookCenter(const glm::vec3& center) {
			this->m_lookCenter = center;
		}
		inline void setUpVector(const glm::vec3& upVec) {
			this->m_upVector = upVec;
		}
		inline void setDistance(const float dis) {
			this->m_distance = dis >= MIN_DISTANCE ? dis : MIN_DISTANCE;
		}
		inline void setFOVInDegree(const float fovDeg) {
			this->m_fovDeg = fovDeg;
		}
		inline void distanceOffset(const float offset) {
			this->setDistance(this->m_distance + offset);
		}

	public:
		static glm::vec3 rotateLookCenterAccordingToViewOrg(const glm::vec3& center, const glm::vec3& eye, const glm::mat4& viewMat, const float rad);

	public:
		inline glm::mat4 projMatrix() const {
			return this->m_projMat;
		}
		inline glm::mat4 viewMatrix() const {
			return this->m_viewMat;
		}
		inline glm::vec3 viewOrig() const {
			return this->m_viewOrg;
		}
		inline glm::vec3 lookCenter() const {
			return this->m_lookCenter;
		}
		inline glm::vec3 upVector() const {
			return this->m_upVector;
		}
		inline const glm::mat4 modelMat() const {
			return this->m_modelMat;
		}
		inline float near() const {
			return this->m_near;
		}
		inline float far() const {
			return this->m_far;
		}
		inline float distance() const {
			return this->m_distance;
		}

	public:
		// return the view space corners
		void viewFrustumClipPlaneCornersInViewSpace(const float depth, float* corners) const;

	private:
		glm::vec3 m_viewOrg;
		glm::vec3 m_lookCenter;
		glm::vec3 m_upVector;
		
		glm::mat4 m_viewMat;
		glm::mat4 m_projMat;
		glm::mat4 m_inverseProjMat;
		glm::mat4 m_inverseVPMat;

		int m_viewportWidth;
		int m_viewportHeight;

		float m_distance;
		float m_fovDeg;
		float m_near;
		float m_far;

		const float MIN_DISTANCE;

		glm::mat4 m_modelMat;

	};


}


