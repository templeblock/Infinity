//////////////////////////////////////////////////////////////////////////////////
// OpenGL Mathematics Copyright (c) 2005 - 2006 G-Truc Creation (www.g-truc.net)
//////////////////////////////////////////////////////////////////////////////////
// Author  : Christophe [Groove] Riccio (contact [at] g-truc [point] net)
// Created : 2005-12-21
// Updated : 2006-11-13
// Licence : This source is under GNU LGPL licence
// File    : gtx_projection.h
//////////////////////////////////////////////////////////////////////////////////
// Dependency:
// - GLM core
//////////////////////////////////////////////////////////////////////////////////

#ifndef __gtx_projection_h__
#define __gtx_projection_h__

#if (GLM_GTX_projection == GLM_EXTENSION_DISABLE && defined(GLM_GTX_projection_required))
#error GLM_GTX_projection is disable but required
#endif

// Dependency:
#include "../glm.h"

#if(GLM_INCLUDED(GLM_GTX_projection, GLM_EXTENSION_GTX, GLM_GTX_projection_required))

#define GLM_GTX_projection_supported 

namespace glm
{
    template <typename T> _xvec2<T> projGTX(const _xvec2<T>& x, const _xvec2<T>& Normal); //!< \brief Projects x on Normal (from GLM_GTX_projection extension)
    template <typename T> _xvec3<T> projGTX(const _xvec3<T>& x, const _xvec3<T>& Normal); //!< \brief Projects x on Normal (from GLM_GTX_projection extension)
    template <typename T> _xvec4<T> projGTX(const _xvec4<T>& x, const _xvec4<T>& Normal); //!< \brief Projects x on Normal (from GLM_GTX_projection extension)

#if (GLM_CONFORMANCE == GLM_CONFORMANCE_FRIENDLY)

    template <typename T> _xvec2<T> proj(const _xvec2<T>& x, const _xvec2<T>& Normal){return projGTX(x, Normal);}
    template <typename T> _xvec3<T> proj(const _xvec3<T>& x, const _xvec3<T>& Normal){return projGTX(x, Normal);}
    template <typename T> _xvec4<T> proj(const _xvec4<T>& x, const _xvec4<T>& Normal){return projGTX(x, Normal);}

#endif
}

#endif//included

#endif//__gtx_projection_h__
