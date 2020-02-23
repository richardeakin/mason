#version 330

uniform sampler3D uTex0;

uniform int 	uNumTiles;
uniform int 	uFocusedLayer;
uniform bool  	uTiledAtlasMode;
uniform float 	uScale = 1;

in vec2	vTexCoord;

out vec4 oFragColor;

void main()
{
	int depth = textureSize( uTex0, 0 ).z;

	if( uTiledAtlasMode ) {
		// show layers tiled
		float tiles = float( uNumTiles );
		vec2 uv = vec2( vTexCoord.x, 1 - vTexCoord.y );
		vec2 coord = fract( uv * tiles );
		vec2 cellId = floor( uv * tiles );

		if( int( cellId.y * tiles + cellId.x ) < depth ) {
			float zcoord = ( cellId.x + cellId.y * tiles ) / ( tiles * tiles - 1 );

			// TODO: use enum to pick viewpoint
			vec3 lookup = vec3( coord.x, 1.0f - coord.y, zcoord ); // front on
			// lookup = lookup.zyx; // from right
			oFragColor = texture( uTex0, lookup );
		}
		else {
			oFragColor = vec4( 0, 0, 0, 1 );
		}
	}
	else {
		// show one tile
		float sliceNormalized = float( uFocusedLayer ) / float( depth );
		oFragColor = texture( uTex0, vec3( vTexCoord, sliceNormalized ) );
	}

	oFragColor.rgb *= uScale;
	oFragColor.a = 1.0;
}
