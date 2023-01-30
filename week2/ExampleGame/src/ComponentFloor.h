
#ifndef COMPONENTFLOOR_H
#define COMPONENTFLOOR_H

#include "W_Sprite.h"
#include "ComponentRenderable.h"

namespace week2
{
	class ComponentFloor : public Common::ComponentRenderable
	{
	public:
		//------------------------------------------------------------------------------
		// Public methods.
		//------------------------------------------------------------------------------
		ComponentFloor();
		virtual ~ComponentFloor();

		virtual const std::string ComponentID() { return std::string("GOC_RenderableMesh"); }
		virtual void Update(float p_fDelta) {}

		//------------------------------------------------------------------------------
		// Public methods for "GOC_Renderable" family of components
		//------------------------------------------------------------------------------
		virtual void Init(const std::string& p_strTexture, const std::string& p_strVertexProgram, const std::string& p_strFragmentProgram, const glm::vec2& p_vDimensions);
		virtual void SyncTransform();
		wolf::Sprite* GetSprite() { return m_pSprite; }

	private:
		//------------------------------------------------------------------------------
		// Private members.
		//------------------------------------------------------------------------------

		wolf::Sprite* m_pSprite;
	};
}

#endif



