
#include <exec/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/glut.h>

PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB = NULL;
PFNGLSHADERSOURCEARBPROC glShaderSourceARB = NULL;
PFNGLCOMPILESHADERARBPROC glCompileShaderARB = NULL;
PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB = NULL;
PFNGLATTACHOBJECTARBPROC glAttachObjectARB = NULL;
PFNGLLINKPROGRAMARBPROC glLinkProgramARB = NULL;
PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB = NULL;
PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB = NULL;

PFNGLGETINFOLOGARBPROC glGetInfoLogARB = NULL;
PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB = NULL;
PFNGLUNIFORM1FPROC glUniform1f = NULL;
PFNGLUNIFORM1IPROC glUniform1i = NULL;
PFNGLUNIFORM2FPROC glUniform2f = NULL;

BOOL GetProcAddresses(void)
{
    glCreateShaderObjectARB = (PFNGLCREATESHADEROBJECTARBPROC) glutGetProcAddress ("glCreateShaderObjectARB");
    if (!glCreateShaderObjectARB)
    {
        printf("This program requires glCreateShaderObjectARB");
        return FALSE;
    }
    glShaderSourceARB = (PFNGLSHADERSOURCEARBPROC) glutGetProcAddress ("glShaderSourceARB");
    if (!glShaderSourceARB)
    {
        printf("This program requires glShaderSourceARB");
        return FALSE;
    }
    glCompileShaderARB = (PFNGLCOMPILESHADERARBPROC) glutGetProcAddress ("glCompileShaderARB");
    if (!glCompileShaderARB)
    {
        printf("This program requires glCompileShaderARB");
        return FALSE;
    }
    glCreateProgramObjectARB = (PFNGLCREATEPROGRAMOBJECTARBPROC) glutGetProcAddress ("glCreateProgramObjectARB");
    if (!glCreateProgramObjectARB)
    {
        printf("This program requires glCreateProgramObjectARB");
        return FALSE;
    }
    glAttachObjectARB = (PFNGLATTACHOBJECTARBPROC) glutGetProcAddress ("glAttachObjectARB");
    if (!glAttachObjectARB)
    {
        printf("This program requires glAttachObjectARB");
        return FALSE;
    }
    glLinkProgramARB = (PFNGLLINKPROGRAMARBPROC) glutGetProcAddress ("glLinkProgramARB");
    if (!glLinkProgramARB)
    {
        printf("This program requires glLinkProgramARB");
        return FALSE;
    }
    glUseProgramObjectARB = (PFNGLUSEPROGRAMOBJECTARBPROC) glutGetProcAddress ("glUseProgramObjectARB");
    if (!glUseProgramObjectARB)
    {
        printf("This program requires glUseProgramObjectARB");
        return FALSE;
    }
    glGetUniformLocationARB = (PFNGLGETUNIFORMLOCATIONARBPROC) glutGetProcAddress ("glGetUniformLocationARB");
    if (!glGetUniformLocationARB)
    {
        printf("This program requires glGetUniformLocationARB");
        return FALSE;
    }

    glGetInfoLogARB = (PFNGLGETINFOLOGARBPROC) glutGetProcAddress ("glGetInfoLogARB");
    if (!glGetInfoLogARB)
    {
        printf("This program requires glGetInfoLogARB");
        return FALSE;
    }
    glGetObjectParameterivARB = (PFNGLGETOBJECTPARAMETERIVARBPROC) glutGetProcAddress ("glGetObjectParameterivARB");
    if (!glGetObjectParameterivARB)
    {
        printf("This program requires glGetObjectParameterivARB");
        return FALSE;
    }
    glUniform1f = (PFNGLUNIFORM1FPROC) glutGetProcAddress ("glUniform1f");
    if (!glUniform1f)
    {
        printf("This program requires glUniform1f");
        return FALSE;
    }
    glUniform1i = (PFNGLUNIFORM1IPROC) glutGetProcAddress ("glUniform1i");
    if (!glUniform1i)
    {
        printf("This program requires glUniform1i");
        return FALSE;
    }
    glUniform2f = (PFNGLUNIFORM2FPROC) glutGetProcAddress ("glUniform2f");
    if (!glUniform2f)
    {
        printf("This program requires glUniform2f");
        return FALSE;
    }

    return TRUE;
}
