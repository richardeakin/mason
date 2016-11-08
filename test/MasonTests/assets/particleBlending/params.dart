import 'cinder.dart';

enum BlendFunc {
    GL_ZERO,
    GL_ONE,
    GL_ONE_MINUS_SRC_COLOR,
    GL_DST_COLOR,
    GL_ONE_MINUS_DST_COLOR,
    GL_SRC_ALPHA,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_DST_ALPHA,
    GL_ONE_MINUS_DST_ALPHA,
    GL_CONSTANT_COLOR,
    GL_ONE_MINUS_CONSTANT_COLOR,
    GL_CONSTANT_ALPHA,
    GL_ONE_MINUS_CONSTANT_ALPHA,
    GL_SRC_ALPHA_SATURATE,
    GL_SRC1_COLOR,
    GL_ONE_MINUS_SRC_COLOR,
    GL_SRC1_ALPHA,
    GL_ONE_MINUS_SRC_ALPHA
}

enum DepthFunc {
    GL_NEVER,
    GL_LESS,
    GL_EQUAL,
    GL_LEQUAL,
    GL_GREATER,
    GL_NOTEQUAL,
    GL_GEQUAL,
    GL_ALWAYS
}

var params = {
    'numParticlesA' : 40,
    'worldDims' : new vec3( 10, 10, 10 ),
    'backgroundColor' : new Color( 0.2, 0.2, 0.2 ),
    'groups' : [
        {
            'type' : 'particles',
            'pos' : new vec3( 0, 1, 2 ),
            'color' : new Color( 0.8, 0.4, 0.4, 0.5 ),
            'depthTest' : true,
            'depthWrite' : true,
            'enableBlend' : true,
            'depthFunc' : DepthFunc.GL_ALWAYS,
            'blendMode' : 'additive'
        }
        ,
        {
            'type' : 'particles',
            'pos' : new vec3( 0, 1.0, 1 ),
            'color' : new Color( 0.2, 0.5, 0.8, 0.5 ),
            'depthTest' : true,
            'depthWrite' : false,
            'depthFunc' : DepthFunc.GL_LESS,
            'enableBlend' : true,
            'blendMode' : 'additive'
        }
        ,
        {
            'type' : 'particles',
            'pos' : new vec3( 0, 1.0, 0.5 ),
            'color' : new Color( 0.2, 0.5, 0.5, 0.3 ),
            'depthTest' : true,
            'depthWrite' : false,
            'depthFunc' : DepthFunc.GL_GREATER,
            'enableBlend' : true,
            'blendMode' : 'additive'
        }
    ]
};

void main()
{
    toCinder( params );
}
