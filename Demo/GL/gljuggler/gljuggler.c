#include <fcntl.h>
//#include <stdlib.h>

//#include <stdarg.h>
#include <GL/gl.h>
//#include <GL/glx.h>

//#include <stdio.h>
//#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <signal.h>
//#include <string.h>
//#include <stdarg.h>
#include <unistd.h>

#include <SDL/SDL.h>

#ifdef __AROS__
typedef LONG int32;
typedef ULONG uint32;
#else
typedef int int32;
typedef unsigned int uint32;
#endif

#define SWAPTIME 20

struct pic {
unsigned char *image;
int w,h;
};


int readppm(char *name,struct pic *pic);
void saveppm(char *fn);

static GLint objectlists;


// position/scaling settings
#define SCALE 2.0 // size of each jewel

static GLfloat lightpos[4] = {-2.0, 4.0, 4.0, 0.0};
static GLfloat white[4] = {1.0, 1.0, 1.0, 1.0};
static GLfloat blue[4] = {0.0, 0.0, 1.0, 1.0};

#define GS .5
static GLfloat specular[4] = {GS, GS, GS, 1.0};
#define GD 1.0
static GLfloat diffuse[4] = {GD, GD, GD, 1.0};



// Juggler init stuff
/*
   This model code was taken from Michael Birkin's java recreation of the
   Amiga juggler demo. I just ported some of the model generation code from
   java to c. The link to Michael's page is here:
   http://meatfighter.com/juggler/

   Ya gotta love open source. Derivative works and all...
*/


struct material {
	int color;
	float scale;
};
struct material MIRROR = {0xffffff, 1.0},
	TORSO = {0xE51715, 1.05},
	SKIN = {0xF2ADAB, 1.05},
	EYE = {0x1E1B94, 1.4},
	HAIR = {0x261117, 1.4};

#define NUMSPHERES 86
struct sphere {
	float center[3];
	float radius;
	GLfloat color[4];
} spheres[86];

void newSphere(int num, float x, float y, float z, float radius, struct material m)
{
struct sphere *s = spheres+num;
float r, g, b;

	s->center[0] = x;
	s->center[1] = y;
	s->center[2] = z;

	s->radius = radius;
	r = m.scale * ((m.color>>16)&255) / 255.0;
	g = m.scale * ((m.color>>8)&255) / 255.0;
	b = m.scale * ((m.color)&255) / 255.0;
	s->color[0] = r;
	s->color[1] = g;
	s->color[2] = b;
	s->color[3] = 1.0;
}

void createscene(void)
{
int i, j;
	// juggling balls 2 -- 4
	// body (hips to chest) 5 -- 12
	// head 13
	// neck 14
	// left leg (15 -- 31)
	// right leg (32 -- 48)
	// left arm (49 -- 65)
	// right arm (66 -- 82)
	// left eye 83
	// right eye 34
	// hair 85

//	scene[0] = new Ground(YELLOW_MATTE, GREEN_MATTE);
//	newSphere(1, 0, 0, 0, 1E6, new SkyMaterial());
	for(i = 2; i <= 4; i++) {
		newSphere(i, 110, 0, 0, 14, MIRROR);
	}
	for(i = 5; i <= 12; i++) {
		double percent = (i - 5) / 7.0;
		newSphere(i, 151, 85 + 32 * percent, -151, 16 + 4 * percent,
				TORSO);
	}
	newSphere(13, 151, 155, -151, 14, SKIN);
	newSphere(14, 151, 140, -151, 5, SKIN);
		for(i = 15; i <= 22; i++) {
		for(j = 0; j < 4; j++) {
			newSphere(i + 17 * j, 0, 0, 0, 2.5 + 2.5 * (i - 15) / 7.0,
					SKIN);
		}
	}
	for(i = 23; i <= 31; i++)
		for(j = 0; j < 4; j++)
			newSphere(i + 17 * j, 0, 0, 0, 5, SKIN);

	newSphere(83, 142, 154, -144, 4, EYE);
	newSphere(84, 142, 154, -144, 4, EYE);
	newSphere(85, 152, 156, -151, 14, HAIR);

}


#define JUGGLE_X0  -182
#define JUGGLE_X1  -108
#define JUGGLE_Y0    88
#define JUGGLE_H_Y  184

#define JUGGLE_H_VX  ((JUGGLE_X0 - JUGGLE_X1) / 60.0)
#define JUGGLE_L_VX  ((JUGGLE_X1 - JUGGLE_X0) / 30.0)

#define JUGGLE_H_H  (JUGGLE_H_Y - JUGGLE_Y0)
#define JUGGLE_H_VY  (4.0 * JUGGLE_H_H / 60.0)
#define JUGGLE_G  (JUGGLE_H_VY * JUGGLE_H_VY / (2.0 * JUGGLE_H_H))

#define JUGGLE_L_VY  (0.5 * JUGGLE_G * 30.0)

#define HIPS_MAX_Y  85
#define HIPS_MIN_Y  81

#define HIPS_ANGLE_MULTIPLIER  (2.0 * M_PI / 30.0)

float magnitude(float *v)
{
	return sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

void normalize(float *v)
{
float d = 1.0 / magnitude(v);
	v[0] *= d;
	v[1] *= d;
	v[2] *= d;
}

void normalize2(float *dv, float *sv)
{
float d = 1.0 / magnitude(sv);
	dv[0] = d*sv[0];
	dv[1] = d*sv[1];
	dv[2] = d*sv[2];
}

void ray(float *a, float *b, float s)
{
int i;
	for(i=0;i<3;++i)
		a[i] += b[i] * s;
}

void ray2(float *a, float *b, float *c, float s)
{
int i;
	for(i=0;i<3;++i)
		a[i] = b[i] + c[i] * s;
}

void subtract(float *a, float *b)
{
int i;
	for(i=0;i<3;++i)
		a[i] -= b[i];
}

void subtract2(float *a, float *b, float *c)
{
int i;
	for(i=0;i<3;++i)
		a[i] = b[i] - c[i];
}

void scale(float *a, float s)
{
int i;
	for(i=0;i<3;++i)
		a[i] *= s;
}

 // q = o + x u + y v
void map(float *q, float *o, float *u, float *v, float x, float y)
{
float i = o[0] + x * u[0] + y * v[0];
float j = o[1] + x * u[1] + y * v[1];
float k = o[2] + x * u[2] + y * v[2];
	q[0] = i;
	q[1] = j;
	q[2] = k;
}

// q = o + y v + z w
void mapYZ(float *q, float *o, float *v, float *w, float y, float z)
{
float i = o[0] + y * v[0] + z * w[0];
float j = o[1] + y * v[1] + z * w[1];
float k = o[2] + y * v[2] + z * w[2];
	q[0] = i;
	q[1] = j;
	q[2] = k;
}

// a = b x c
void cross(float *a, float *b, float *c)
{
float x = b[1] * c[2] - b[2] * c[1];
float y = b[2] * c[0] - b[0] * c[2];
float z = b[0] * c[1] - b[1] * c[0];
	a[0] = x;
	a[1] = y;
	a[2] = z;  
}

float temps[16][3];

void updateAppendage(int sceneIndex,
	  float *p, float *q, float *w, float A, float B,
	  int countA, int countB)
{
float *U = temps[0];
float *V = temps[1];
float *W = temps[2];
float *j = temps[3];
float *d = temps[4];
int i;

	subtract2(V, q, p);
	float D = magnitude(V);
	float inverseD = 1.0 / D;
	scale(V, inverseD);

	normalize2(W, w);
	cross(U, V, W);

	float A2 = A * A;

	float y = 0.5 * inverseD * (A2 - B * B + D * D);
	float square = A2 - y * y;
	float x = sqrt(square);

	map(j, p, U, V, x, y);

	subtract2(d, j, p);
	scale(d, 1.0 / countA);
	for(i = 0; i <= countA; i++) {
		float *center = spheres[sceneIndex + i].center;
		ray2(center, p, d, i);
	}

	subtract2(d, j, q);
	scale(d, 1.0 / countB);
	for(i = 0; i < countB; i++) {
		float *center = spheres[countA + 1 + sceneIndex + i].center;
		ray2(center, q, d, i);
	}
}


void updatescene(int frameIndex, float t)
{
struct sphere *sphere;
int i;

	// mirrored juggling balls (2 -- 4)
	float T = (30 + frameIndex + t);
	sphere = spheres+3;
	sphere->center[2] = JUGGLE_X1 + JUGGLE_H_VX * T;
	sphere->center[1] = JUGGLE_Y0 + (JUGGLE_H_VY - 0.5 * JUGGLE_G * T) * T;

	T = frameIndex + t;
	sphere = spheres+4;
	sphere->center[2] = JUGGLE_X1 + JUGGLE_H_VX * T;
	sphere->center[1] = JUGGLE_Y0 + (JUGGLE_H_VY - 0.5 * JUGGLE_G * T) * T;

	sphere = spheres+2;
	sphere->center[2] = JUGGLE_X0 + JUGGLE_L_VX * T;
	sphere->center[1] = JUGGLE_Y0 + (JUGGLE_L_VY - 0.5 * JUGGLE_G * T) * T;

	// body (hips to chest) 5 -- 12
	float angle = HIPS_ANGLE_MULTIPLIER * T;
	float oscillation = 0.5 * (1.0 + cos(angle));

	float *o = temps[5];
	o[0] = 151;
	o[1] = HIPS_MIN_Y + (HIPS_MAX_Y - HIPS_MIN_Y) * oscillation;
	o[2] = -151;
		
	float *u = temps[6];
	float *v = temps[7];
	float *w = temps[8];

	v[0] = 0;
	v[1] = 70;
	v[2] = (HIPS_MIN_Y - HIPS_MAX_Y) * sin(angle);
	normalize(v);

	u[0] = 0;
	u[1] = v[2];
	u[2] = -v[1];

	w[0] = 1;
	w[1] = 0;
	w[2] = 0;

	for(i = 5; i <= 12; i++)
	{
		float percent = (i - 5) / 7.0;
		sphere = spheres+i;
		ray2(sphere->center, o, v, 32 * percent);
	}
	sphere = spheres + 13;
	ray2(sphere->center, o, v, 70);
	sphere = spheres+ 14;
	ray2(sphere->center, o, v, 55);

	// left leg (15 -- 32)
	float *p = temps[9];
	p[0] = 159;
	p[1] = 2.5;
	p[2] = -133;
	float *q = temps[10];
	mapYZ(q, o, v, u, -9, -16);
	updateAppendage(15, p, q, u, 42.58, 34.07, 8, 8);

	// right leg (32 -- 48)
	p[0] = 139;
	p[1] = 2.5;
	p[2] = -164;
	mapYZ(q, o, v, u, -9, 16);
	updateAppendage(32, p, q, u, 42.58, 34.07, 8, 8);

	// left arm (49 -- 65)
	float *n = temps[11];
	float armAngle = -0.35 * oscillation;
	p[0] = 69 + 41 * cos(armAngle);
	p[1] = 60 - 41 * sin(armAngle);
	p[2] = -108;
	mapYZ(q, o, v, u, 45, -19);		
	mapYZ(n, o, v, u, 45.41217, -19.91111);
	subtract(n, q);
	updateAppendage(49, p, q, n, 44.294, 46.098, 8, 8);

	// right arm (66 -- 82)
	p[2] = -182;
	mapYZ(q, o, v, u, 45, 19);
	mapYZ(n, o, v, u, 45.41217, 19.91111);
	subtract2(n, q, n);
	updateAppendage(66, p, q, n, 44.294, 46.098, 8, 8);

	// left eye (83)
	sphere = spheres+83;
	mapYZ(sphere->center, o, v, u, 69, -7);
	sphere->center[0] = 142;

	// right eye (84)
	sphere = spheres+84;
	mapYZ(sphere->center, o, v, u, 69, 7);
	sphere->center[0] = 142; 

	// hair (85)
	sphere = spheres+85;
	ray2(sphere->center, o, v, 71);
	sphere->center[0] = 152;
}

//





int mousex,mousey,sizex,sizey;


struct timeval starttime;

void inittime()
{
	gettimeofday(&starttime,0);

}
uint32 gtime()
{
struct timeval tv;
int s,m;


	gettimeofday(&tv,0);
	s=tv.tv_sec-starttime.tv_sec;
	m=tv.tv_usec-starttime.tv_usec;
	if(m<0) {m+=1000000;--s;}
	return s*1000+m/1000;
}



uint32 torgba(unsigned int r,unsigned int g,unsigned int b,unsigned int a)
{
	r&=255;
	g&=255;
	b&=255;
	a&=255;
	return (a<<24) | (r<<0) | (g<<8) | (b<<16);
}




SDL_Surface *setvideomode(int w, int h)
{
SDL_Surface *screen;
	screen = SDL_SetVideoMode(w, h, 24, SDL_OPENGL | SDL_RESIZABLE);
	if(!screen)
	{
		fprintf(stderr, "Couldn't set %dx%d GL video mode: %s\n",
			sizex, sizey, SDL_GetError());
		SDL_Quit();
		exit(2);
	}
	return screen;
}

static void resize( unsigned int width, unsigned int height )
{
	sizex=width;
	sizey=height;

	setvideomode(width, height);

	glViewport(0, 0, (GLint) width, (GLint) height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	float h;
	h = (GLfloat) height / (GLfloat) width;
	glFrustum(-1.0, 1.0, -h, h, 5.0, 800000.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0, 0.0, -40.0);
}



void setmaterial(GLfloat *color)
{
float shiny=25.0;
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
	glMaterialfv(GL_FRONT, GL_SPECULAR, white);
	glMaterialfv(GL_FRONT, GL_SHININESS, &shiny);
}


#define X .525731112119133606
#define Z .850650808352039932

static GLfloat vdata [12][3] = {
{-X, 0.0, Z}, {X, 0.0, Z}, {-X, 0.0, -Z},
{X, 0.0, -Z}, {0.0, Z, X}, {0.0, Z, -X},
{0.0, -Z, X}, {0.0, -Z, -X}, {Z, X, 0.0},
{-Z, X, 0.0}, {Z, -X, 0.0}, {-Z, -X, 0.0}
};

static GLuint tindices [20][3] = {
 {1, 4, 0}, {4, 9, 0},
 {4, 5, 9}, {8, 5, 4},
 {1, 8, 4}, {1, 10, 8},
 {10, 3, 8}, {8, 3, 5},
 {3, 2, 5}, {3, 7, 2},
 {3, 10, 7}, {10, 6, 7},
 {6, 11, 7}, {6, 0, 11},
 {6, 1, 0}, {10, 1, 6},
 {11, 0, 9}, {2, 11, 9},
 {5, 2, 9}, {11, 2, 7}
};

void mdup3(float *a,float *b)
{
	a[0] = b[0];
	a[1] = b[1];
	a[2] = b[2];
}

void mid(float *a,float *b,float *c)
{
float d;
	a[0]=(b[0] + c[0])/2.0;
	a[1]=(b[1] + c[1])/2.0;
	a[2]=(b[2] + c[2])/2.0;
	d=1.0/sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);
	a[0]*=d;
	a[1]*=d;
	a[2]*=d;
}

int norm2(float *p1,float *p2,float *p3)
{
float nx,ny,nz;
float x1,y1,z1;
float x2,y2,z2;

	x1=p2[0]-p1[0];
	y1=p2[1]-p1[1];
	z1=p2[2]-p1[2];

	x2=p3[0]-p1[0];
	y2=p3[1]-p1[1];
	z2=p3[2]-p1[2];

	nx=y1*z2-y2*z1;
	ny=x2*z1-x1*z2;
	nz=x1*y2-x2*y1;
if(nz<0) return 0; // don't bother with back faces...
	glNormal3f(nx,ny,nz);
	return 1;
}


void makeicosahedron(int sub,float scale)
{
struct tri {
	float p[3][3];
	int depth;
} tris[128], *sp;
int i,j;

	sp = tris;
	for(i=0;i<20;++i)
	{
		sp->depth = 0;
		for(j=0;j<3;++j)
			mdup3(sp->p[j], vdata[tindices[i][j]]);
		++sp;
	}
	glEnable(GL_NORMALIZE);
	glBegin(GL_TRIANGLES);

	while(sp>tris)
	{
		int depth;
		struct tri m, t;
		t = *(--sp);
		depth = sp->depth;
		if(depth==sub)
		{
//			if(!norm2(t.p[0], t.p[1], t.p[2]))
//				continue;
			for(i=0;i<3;++i)
			{
				glNormal3f(t.p[i][0],t.p[i][1],t.p[i][2]);
				glVertex3f(t.p[i][0]*scale,t.p[i][1]*scale,t.p[i][2]*scale);
			}
			continue;
		}
		++depth;

		mid(m.p[0], t.p[0], t.p[1]);
		mid(m.p[1], t.p[1], t.p[2]);
		mid(m.p[2], t.p[2], t.p[0]);

		sp->depth = depth;
		mdup3(sp->p[0], m.p[2]);
		mdup3(sp->p[1], t.p[0]);
		mdup3(sp->p[2], m.p[0]);
		++sp;

		sp->depth = depth;
		mdup3(sp->p[0], m.p[0]);
		mdup3(sp->p[1], t.p[1]);
		mdup3(sp->p[2], m.p[1]);
		++sp;

		sp->depth = depth;
		mdup3(sp->p[0], m.p[1]);
		mdup3(sp->p[1], t.p[2]);
		mdup3(sp->p[2], m.p[2]);
		++sp;

		sp->depth = depth;
		mdup3(sp->p[0], m.p[0]);
		mdup3(sp->p[1], m.p[1]);
		mdup3(sp->p[2], m.p[2]);
		++sp;
	}
	glEnd();
}


void makeuvsphere(float size)
{
#define USIDES 15
#define VSIDES 9
float x[USIDES],z[USIDES];
int i,j,t,o;
float a;
float c1,s1,c2,s2;
	for(i=0;i<USIDES;++i)
	{
		a=i*3.1415927*2.0/USIDES;
		x[i]=cos(a)*size;
		z[i]=sin(a)*size;
	}

	glEnable(GL_NORMALIZE);

	for(i=0;i<VSIDES;++i)
	{
		a=i*3.1415927/VSIDES;
		c1=cos(a);
		s1=sin(a);
		a=(i+1)*3.1415927/VSIDES;
		c2=cos(a);
		s2=sin(a);
		glBegin(GL_QUAD_STRIP);
		for(j=0;j<USIDES+1;++j)
		{
			t=(j<USIDES) ? j : j-USIDES;
			if(j)
			{
				float s,c;
				a=(i+0.5)*3.1415927/VSIDES;
				c=cos(a);
				s=sin(a);
				a=(j-0.5)*3.1415927*2.0/USIDES;
				glNormal3f(cos(a)*s,c,sin(a)*s);
			}
			glVertex3f(x[t]*s2,c2*size,z[t]*s2);
			glVertex3f(x[t]*s1,c1*size,z[t]*s1);
			o=t;
		}
		glEnd();
	}
}





void setgrey(float v)
{
float col[4]={v,v,v,v};
	glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE, col);

}


float cursorrot;
int cposx=0,cposy=0;


inline double dist(double dx, double dy)
{
	return sqrt(dx*dx + dy*dy);
}


GLuint bg_texture[1];
#define FWIDTH 4
#define FHEIGHT FWIDTH

int got_bg = -1;
void get_bg(void)
{
//struct pic pic;
//int res;
unsigned char *rgb, *po;
int x, y;


//	if(got_bg >= 0) return;
	if(got_bg == 1)
	{
		glDeleteTextures(1, bg_texture);
	}
	got_bg = 0;

//	res = jpeg_unpack_sized(&pic, "background.jpg", 512, 512);
//	if(res<0) return;
	rgb = malloc(FWIDTH * FHEIGHT * 3);
	for(y=0;y<FHEIGHT;++y)
	{
		po = rgb + y*FWIDTH*3;
		for(x=0;x<FWIDTH;++x)
		{
			po[2] = 0;
			po[1] = 255;
#define F4 (FWIDTH/4)
			po[0] = (((x-F4)^(y+F4)) & (FWIDTH/2)) ? 255 : 0;
			po += 3;
		}
	}

	glGenTextures(1, bg_texture);
	glBindTexture( GL_TEXTURE_2D, bg_texture[0]);

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);

	glTexImage2D( GL_TEXTURE_2D, 0, 4, FWIDTH, FHEIGHT, 0, GL_RGB,
				GL_UNSIGNED_BYTE, rgb );


	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
		GL_LINEAR_MIPMAP_LINEAR);
	free(rgb);
	got_bg = 1;
}



static void draw(void)
{
int i;
double t;
static int tt = 0;
	updatescene((tt/3) % 30, (tt%3)*.33333);
	++tt;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();
	glRotatef(-90 + tt*.2, 0.0, -1.0, 0.0);
	glLightfv(GL_LIGHT0, GL_POSITION, lightpos);


	glEnable(GL_LIGHT0);
	glDisable(GL_LIGHT1);
	glDisable(GL_LIGHT2 );
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
//	glDisable(GL_CULL_FACE);


	glPushMatrix();
//	glTranslatef(0.0, -1.0, 0.0);
	t = .05;
	glScalef(t, t, t);
	glTranslatef(0.0, -100.0, 0.0);
//	glRotatef(tt*.2, 0.0, 1.0, 0.0);

	for(i=2;i<NUMSPHERES;++i)
	{
		struct sphere *s;
		s = spheres + i;

		glPushMatrix();
		glTranslatef(s->center[0] - 150.0, s->center[1], s->center[2] + 150.0);
		t = s->radius * 0.5;
		glScalef(t, t, t);

		setmaterial(s->color);
		glCallList(objectlists+2);
		glPopMatrix();
	}

// sky
	setmaterial(blue);
	glBegin(GL_QUADS);
	glNormal3f(0.0, 1.0, 0.0);
#define BIG 10000000.0
#define YPOS 200.0
	glVertex3f( BIG, YPOS, -BIG);
	glVertex3f( BIG, YPOS,  BIG);
	glVertex3f(-BIG, YPOS,  BIG);
	glVertex3f(-BIG, YPOS, -BIG);
	glEnd();

// floor
	if(got_bg == 1)
	{
		float u=0.5, v=0.5, t;
		int i;

		glDisable(GL_LIGHTING);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_NORMALIZE);
		glBindTexture(GL_TEXTURE_2D, bg_texture[0] );
		glBegin(GL_QUADS);

		glNormal3f(0.0, 0.0, 1.0);
		for(i=0;i<4;++i)
		{
#define VV (BIG/400.0)
			glTexCoord2f(VV*(u + .5), VV*(v + .5));
#define TT BIG
			glVertex3f(u * TT, 0.0, -v * TT);

			t = u;
			u = -v;
			v = t;
		}

		glEnd();
		glEnable(GL_LIGHTING);
		glDisable(GL_TEXTURE_2D);
	}

	glPopMatrix();




//	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);


	glPopMatrix();

	SDL_GL_SwapBuffers();


}

void showmatrix(float *f)
{
	printf("%f %f %f %f\n",f[0],f[4],f[8],f[12]);
	printf("%f %f %f %f\n",f[1],f[5],f[9],f[13]);
	printf("%f %f %f %f\n",f[2],f[6],f[10],f[14]);
	printf("%f %f %f %f\n",f[3],f[7],f[11],f[15]);
}

int chainreaction;


void randomvector(float *x,float *y,float *z)
{
int ix,iy,iz;
float d;

	do
	{
		ix=(random()&1023)-512;
		iy=(random()&1023)-512;
		iz=(random()&1023)-512;
	} while(ix==0 && iy==0 && iz==0);
	d=1.0/sqrt((float)(ix*ix+iy*iy+iz*iz));
	*x=ix*d;
	*y=iy*d;
	*z=iz*d;
*x=*z=0;*y=1.0;
}



#define MAXCODES 64
static int downcodes[MAXCODES];
static int pressedcodes[MAXCODES];
static int numcodes=0,numpressed=0;

void processkey(int key,int state)
{
int i;
	if(state)
	{
		if(numpressed<MAXCODES) pressedcodes[numpressed++]=key;
		for(i=0;i<numcodes;++i)
			if(downcodes[i]==key) return;
		if(numcodes<MAXCODES)
			downcodes[numcodes++]=key;
	} else
	{
		for(i=0;i<numcodes;)
		{
			if(downcodes[i]==key)
				downcodes[i]=downcodes[--numcodes];
			else
				++i;
		}

	}
}
int IsDown(int key)
{
int i;
	for(i=0;i<numcodes;++i)
		if(downcodes[i]==key) return 1;
	return 0;
}

int WasPressed(int key)
{
int i;
	for(i=0;i<numpressed;++i)
		if(pressedcodes[i]==key) return 1;
	return 0;
}

void typedkey(int key)
{
	if(key<0 || key>0x7f) return;
}


int downx,downy;
int isdown;
void down(int x,int y)
{
	downx=x;
	downy=y;
	isdown=1;
}

#define ABS(x) (((x)<0) ? (-(x)) : (x))
void moved(int x,int y)
{
}

void up(int x,int y)
{
	isdown=0;
}


#define INTERVAL 5

int hc=0;
void thandler(int val)
{
	signal(SIGALRM,thandler);
	++hc;
}
inline double min(double v1, double v2)
{
	return v1<v2 ? v1 : v2;
}

static void event_loop( void )
{
char exitflag=0;
int t;
int nexttime;
int nframes=0;
struct itimerval itval;
SDL_Event event;
int code;
int isdown = 0;

#ifdef __AROS__
#else
	itval.it_interval.tv_sec=itval.it_value.tv_sec=0;
	itval.it_interval.tv_usec=itval.it_value.tv_usec=10000;
	thandler(0);
	setitimer(ITIMER_REAL,&itval,NULL);

	inittime();
	nexttime=INTERVAL*1000;
#endif
	while(!exitflag)
	{
#ifdef __AROS__
        usleep(INTERVAL*1000);
#else
		int hct;
		if(!hc) pause();
		hct=hc;
		hc=0;

		t=gtime();
		if(0 && t>=nexttime)
		{
			printf("%3d frames in %d seconds:%6.2f frames/second\n",
				nframes,INTERVAL,(float)nframes/INTERVAL);
			nexttime+=INTERVAL*1000;
			nframes=0;
		}
#endif
		draw();

		++nframes;

		numpressed=0;

		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
			case SDL_MOUSEMOTION:
				mousex=event.motion.x;
				mousey=event.motion.y;
				moved(mousex, mousey);
				break;
			case SDL_MOUSEBUTTONDOWN:
				mousex=event.button.x;
				mousey=event.button.y;
				down(mousex, mousey);
				isdown = 1;
				break;
			case SDL_MOUSEBUTTONUP:
				mousex=event.button.x;
				mousey=event.button.y;
				up(mousex, mousey);
				isdown = 0;
				break;
			case SDL_KEYDOWN:
				code=event.key.keysym.sym;
				if(code==SDLK_ESCAPE) exitflag=1;
#ifdef __AROS__
				if(code=='g') saveppm("T:gljuggler-save.ppm");
#else
				if(code=='g') saveppm("/ram/save.ppm");
#endif
				processkey(code, 1);
				break;
			case SDL_KEYUP:
				code=event.key.keysym.sym;
				processkey(code, 0);
				break;
			case SDL_VIDEORESIZE:
				resize(event.resize.w, event.resize.h);
				break;
			case SDL_VIDEOEXPOSE:
				SDL_GL_SwapBuffers();
				break;
			case SDL_QUIT:
				exitflag=1;
				break;				
// handle resize
			}
		}
	}
}

int ppmh;
int ppmin;
unsigned char ppmbuff[2048],*ppmp;
int ppmci()
{
	if(ppmin==0)
	{
		ppmin=read(ppmh,ppmbuff,sizeof(ppmbuff));
		ppmp=ppmbuff;
	}
	if(ppmin<=0) return -1;
	--ppmin;
	return *ppmp++;
}

void ppmline(unsigned char *put)
{
int c;
	while((c=ppmci())>=0)
		if(c==0x0a) break;
		else *put++=c;
	*put=0;
}

int readppm(char *name,struct pic *pic)
{
unsigned char line[8192];
int w,h;
unsigned int i,j;
unsigned char *put;

	ppmin=0;
	ppmh=open(name,O_RDONLY);
	if(ppmh<0) return 0;
	ppmline(line);
	if(strcmp((char *)line,"P6")) {close(ppmh);return 0;}
	ppmline(line);
	if(sscanf((char *)line,"%d %d",&w,&h)!=2) {close(ppmh);return 0;}
	pic->w=w;
	pic->h=h;
	pic->image=put=malloc(w*h*3);
	if(!put) {close(ppmh);return 0;}
	ppmline(line);
	for(j=0;j<h;++j)
	{
		for(i=0;i<w;++i)
		{
			*put++=ppmci();
			*put++=ppmci();
			*put++=ppmci();
		}
	}
	return 1;
}

void saveppm(char *fn)
{
unsigned char temp[16384];

int f;
int y;
int res;

	f = open(fn, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	sprintf((char *)temp, "P6\n%d %d\n255\n", sizex, sizey);
	res = write(f, (char *)temp, strlen((char *)temp));

	for(y=0;y<sizey;++y)
	{

		glReadPixels(0, sizey-1-y, sizex, 1, GL_COLOR_INDEX | GL_RGB,
			GL_UNSIGNED_BYTE, temp);

		res = write(f, temp, 3*sizex);
	}
	close(f);
}

int main( int argc, char *argv[] )
{
SDL_Surface *screen;
int i;
int tx, ty;

//	if(fork()) return;

	srandom(gtime());

	SDL_Init(SDL_INIT_VIDEO);
	sizex = 512;
	sizey = 512;

	if(argc>1 && sscanf(argv[1], "%dx%d", &tx, &ty)==2)
	{
		sizex = tx;
		sizey = ty;
	}

	screen = setvideomode(sizex, sizey);
	SDL_WM_SetCaption("gljuggler", "gljuggler");

	resize(sizex, sizey);

//	glLightfv(GL_LIGHT0, GL_POSITION, lightpos);
	glEnable(GL_CULL_FACE);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	glLightfv(GL_LIGHT0,GL_SPECULAR,specular);
	glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuse);



	glEnable(GL_DEPTH_TEST);

	glShadeModel( GL_SMOOTH );
	glClearColor( 0.0, 0.0, 0.0, 1.0 );
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glDisable(GL_BLEND);
//	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glBlendFunc(GL_ONE, GL_ONE);

	glLineWidth(2.0);
	glDisable(GL_LINE_SMOOTH);

	objectlists = glGenLists(9);

#define NUMICOS 6
	for(i=0;i<=NUMICOS;++i)
	{
		int j;
		j = i;
		if(j>4) j = 4;
		glNewList(objectlists+i, GL_COMPILE);
		makeicosahedron(j,SCALE);
		glEndList();
	}

	glNewList(objectlists+8, GL_COMPILE);
	makeuvsphere(0.9*SCALE); // white
	glEndList();

	createscene();
	get_bg();

	event_loop();
	SDL_Quit();
	return 0;
}
