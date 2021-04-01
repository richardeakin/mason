
vec3 gammaCorrection( in vec3 color )
{
	return pow( color, vec3( 1.0 / 2.2 ) );
}

// Filmic tonemapping from http://filmicgames.com/archives/75
vec3 toneMapUncharted2( in vec3 x )
{
	const float A = 0.15;
	const float B = 0.50;
	const float C = 0.10;
	const float D = 0.20;
	const float E = 0.02;
	const float F = 0.30;

	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}


vec3 toneMap( in vec3 color, in float exposure )
{
	// apply the tone-mapping
	vec3 result = toneMapUncharted2( color * exposure );
	
	// white balance
	const float whiteInputLevel = 10.0;
	vec3 whiteScale			= 1.0 / toneMapUncharted2( vec3( whiteInputLevel ) );
	result					= result * whiteScale;
	
	return result;
}
