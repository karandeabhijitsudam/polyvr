#version 400 compatibility
#define M_PI 3.1415926535897932384626433832795

// gen
in vec3 norm;
in vec4 pos;
in vec2 tcs;
in mat3 miN;
in mat4 miP;
vec3 fragDir;
vec4 color;
uniform vec2 OSGViewportSize;
uniform float rainOffset;
uniform float rainDensity;

uniform sampler2D tex;
//uniform sampler2D tex1;

uniform float camH;

float theta;
float dropsize = 0.1;

void computeDirection() {
	fragDir = normalize( miN * (miP * pos).xyz );
}

// returns the x-z coordinates on plane y = h
// at which the ray fragDir intersects
vec2 planeIntersect(float h){
	vec2 pt = vec2(0.);
	// r = h / cos \phi
	pt = - h * fragDir.xz / fragDir.y ;
	return pt;
}

float hash(vec2 co){
    	return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float gettheta(vec3 d){
	//return acos(d.y/length(d));
	return acos(d.y);
}

bool obstruction(float D){
	float phi = -atan(fragDir.x,fragDir.z);
		
	vec2 texPos = vec2(0.5+sin(phi)*D/(camH),0.5+cos(phi)*D/(camH));	//texture positions
	vec4 texC = texture2D(tex,texPos); 			//RGB value of pixel
	float disCam = texC.r * (512-0.1);			//distance to cam above	
	float thetaReal = atan(D,camH-disCam-0.1);
	
	if (thetaReal < M_PI/2) return true;	

	if (thetaReal < gettheta(fragDir)) return true;
	return false;
}

float getdropsize(in float theta,float distance){
	if (theta < M_PI/2) return dropsize*2*theta/M_PI*2/distance;
	else {
		if (theta > M_PI/2) return dropsize*2*(M_PI-theta)/M_PI*2/distance;
		else return 0;
	}
}

float getOffset(in float rOffset, in float dropdis,in float D) {
	return rOffset*5/sqrt(D); // mod(rOffset*5/100,dropdis);
	//return 0;
}

void computeDepth(vec3 position) {
	vec4 pp = gl_ProjectionMatrix * vec4(position, 1);
	float d = pp.z / pp.w;
	gl_FragDepth = d*0.5 + 0.5;
}

bool isD(float D) {
	float dropdis = 2/(D*D); // horizontal distance between drops
	float dropdisy = rainDensity*6; // vertical distance between drops
	float dropsize = getdropsize(gettheta(fragDir),D);
	float toffset = rainOffset;
	vec2 noise = vec2(floor(atan( fragDir.x, fragDir.z)*180/M_PI/dropdis),floor((D/tan(gettheta(fragDir))+getOffset(toffset,dropdisy,D))/dropdisy));

	float israindropx = mod(atan( fragDir.x, fragDir.z)*180/M_PI+7*hash(noise),dropdis); //phi horizontal in [degree]
	float israindropy = mod(D/tan(gettheta(fragDir))+getOffset(toffset,dropdisy,D)+dropdisy*hash(noise),dropdisy); //height vertical in [m]
	float x = D*sin(atan( fragDir.x, fragDir.z));
	float y = D*cos(atan( fragDir.x, fragDir.z));
	float z = - D*sin(gettheta(fragDir));
	computeDepth(vec3(x,y,z));
//- D/(512-0.1)*sin(gettheta(fragDir)))
	if (gettheta(fragDir)>0.3 && israindropx < dropsize && israindropy < dropsize && !obstruction(D)) return true;
	else return false;

	return true;
}


vec3 checkrad() {
	//might as well incorporate into main()

	vec3 color = vec3(0,0,0.8);
	if (obstruction(1)) color = mix(color,vec3(1,0,0),0.5);	
	if (obstruction(2)) color = mix(color,vec3(0,1,0),0.5);
	if (obstruction(3)) color = mix(color,vec3(0,0,1),0.5);
	if (obstruction(5)) color = mix(color,vec3(1,1,0),0.5);
	if (obstruction(8)) color = mix(color,vec3(0,1,1),0.5);

	//if (obstruction(1)) return vec3(1,0,0);	
	//if (obstruction(2)) return vec3(0,1,0);
	//if (obstruction(3)) return vec3(0,0,1);
	//if (obstruction(5)) return vec3(1,1,0);
	//if (obstruction(8)) return vec3(0,1,1);

	
	//if (atan( fragDir.x, fragDir.z)*180/M_PI<1 && atan( fragDir.x, fragDir.z)*180/M_PI>-2 && gettheta(fragDir)>M_PI/2) return vec3(1,1,1); 
	if (isD(1) || isD(2) || isD(3) ||isD(5) || isD(8)) return color;
	//if (isD(2)) return vec3(0,0,0.8);
	//if (isD(20)) return vec3(0.5,0.5,0.7);
	else discard;
}

void main() {
	computeDirection();

	// \theta is angle of fragDir to zenith
	theta = acos(fragDir.y);

	// vertical line for testing
	//if (fragDir.x < 0.01 && fragDir.x > 0) gl_FragColor = vec4(0,0,0,1);
	//gl_FragColor = vec4(fragDir.yxz, 1.0);

	//if (theta < 0.5*M_PI) discard;
	//if (fragDir.y < 0) discard;

	mat4 m = inverse(gl_ModelViewMatrix);
	vec3 PCam = (m*vec4(0,0,0,1)).xyz;
	vec3 P0 = vec3(0,100,0);
	vec3 T0 = P0-PCam;
	vec3 D0 = normalize( P0-PCam );
	//if (dot(D0,fragDir) < 0.9999 && dot(D0,fragDir) > 0.999) discard;

	//vec3 check = checkrad(fragDir.x, fragDir.y, fragDir.z, D0, T0);
	vec3 check = checkrad();
	gl_FragColor = vec4(check,0.2);
	//computeDepth(vec3(0,0,-2));

	//gl_FragColor = vec4(0,0,0.5, 1.0);
	//gl_FragColor = vec4(-fragDir, 1.0);
}





