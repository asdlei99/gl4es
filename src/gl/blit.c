/* Blit utility function */

#include "blit.h"
#include "../glx/hardext.h"
#include "init.h"

#define SHUT(a) if(!globals4es.nobanner) a

// hacky viewport temporary changes
void pushViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void popViewport();

void gl4es_blitTexture_gles1(GLuint texture, 
    float width, float height, 
    float nwidth, float nheight, 
    float zoomx, float zoomy, 
    float vpwidth, float vpheight, 
    float x, float y, int mode) {

    LOAD_GLES(glClientActiveTexture);

    GLfloat old_projection[16], old_modelview[16], old_texture[16];

    int customvp = (vpwidth>0.0);
    int drawtexok = (hardext.drawtex) && (zoomx==1.0f) && (zoomy==1.0f);

    GLuint old_cli = glstate->texture.client;
    if (old_cli!=0) gles_glClientActiveTexture(GL_TEXTURE0);

    gl4es_glDisable(GL_LIGHTING);
    gl4es_glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    switch (mode) {
        case BLIT_OPAQUE:
            gl4es_glDisable(GL_ALPHA_TEST);
            gl4es_glDisable(GL_BLEND);
            gl4es_glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            break;
        case BLIT_ALPHA:
			gl4es_glEnable(GL_ALPHA_TEST);
			gl4es_glAlphaFunc(GL_GREATER, 0.0f);
            break;
        case BLIT_COLOR:
            gl4es_glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            break;
    }

    if(drawtexok) {
        LOAD_GLES_OES(glDrawTexf);
        LOAD_GLES(glTexParameteriv);
        // setup texture first
        int sourceRect[4] = {0, 0, width, height};
        gles_glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, sourceRect);
        // take x/y of ViewPort into account
        float dx = (customvp)?0.0f:glstate->raster.viewport.x;
        float dy = (customvp)?0.0f:glstate->raster.viewport.y;
        //TODO: do something with width / height of ViewPort?
        // then draw it
        gles_glDrawTexf(x+dx, y+dy, 0.0f, width, height);
    } else {
        LOAD_GLES(glEnableClientState);
        LOAD_GLES(glDisableClientState);
        LOAD_GLES(glVertexPointer);
        LOAD_GLES(glTexCoordPointer);
        LOAD_GLES(glDrawArrays);

        float w2 = 2.0f / (customvp?vpwidth:glstate->raster.viewport.width);
        float h2 = 2.0f / (customvp?vpheight:glstate->raster.viewport.height);
        float blit_x1=x;
        float blit_x2=x+width*zoomx;
        float blit_y1=y;
        float blit_y2=y+height*zoomy;
        GLfloat blit_vert[] = {
            blit_x1*w2-1.0f, blit_y1*h2-1.0f,
            blit_x2*w2-1.0f, blit_y1*h2-1.0f,
            blit_x2*w2-1.0f, blit_y2*h2-1.0f,
            blit_x1*w2-1.0f, blit_y2*h2-1.0f
        };
        GLfloat rw = width/nwidth;
        GLfloat rh = height/nheight;
        GLfloat blit_tex[] = {
            0,  0,
            rw, 0,
            rw, rh,
            0,  rh
        };

        gl4es_glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT | GL_CLIENT_PIXEL_STORE_BIT);
        gl4es_glGetFloatv(GL_TEXTURE_MATRIX, old_texture);
        gl4es_glGetFloatv(GL_PROJECTION_MATRIX, old_projection);
        gl4es_glGetFloatv(GL_MODELVIEW_MATRIX, old_modelview);
        gl4es_glMatrixMode(GL_TEXTURE);
        gl4es_glLoadIdentity();
        gl4es_glMatrixMode(GL_PROJECTION);
        gl4es_glLoadIdentity();
        gl4es_glMatrixMode(GL_MODELVIEW);
        gl4es_glLoadIdentity();

        if(customvp)
            pushViewport(0,0,vpwidth, vpheight);
        
        if(!glstate->clientstate.vertex_array) 
        {
            gles_glEnableClientState(GL_VERTEX_ARRAY);
            glstate->clientstate.vertex_array = 1;
        }
        gles_glVertexPointer(2, GL_FLOAT, 0, blit_vert);
        if(!glstate->clientstate.tex_coord_array[0]) 
        {
            gles_glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glstate->clientstate.tex_coord_array[0] = 1;
        }
        gles_glTexCoordPointer(2, GL_FLOAT, 0, blit_tex);
        for (int a=1; a <hardext.maxtex; a++)
            if(glstate->clientstate.tex_coord_array[a]) {
                gles_glClientActiveTexture(GL_TEXTURE0 + a);
                gles_glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                glstate->clientstate.tex_coord_array[a] = 0;
                gles_glClientActiveTexture(GL_TEXTURE0);
            }
        if(glstate->clientstate.color_array) {
            gles_glDisableClientState(GL_COLOR_ARRAY);
            glstate->clientstate.color_array = 0;
        }
        if(glstate->clientstate.normal_array) {
            gles_glDisableClientState(GL_NORMAL_ARRAY);
            glstate->clientstate.normal_array = 0;
        }
        gles_glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        if(customvp)
            popViewport();

        gl4es_glPopClientAttrib();
        gl4es_glMatrixMode(GL_TEXTURE);
        gl4es_glLoadMatrixf(old_texture);
        gl4es_glMatrixMode(GL_MODELVIEW);
        gl4es_glLoadMatrixf(old_modelview);
        gl4es_glMatrixMode(GL_PROJECTION);
        gl4es_glLoadMatrixf(old_projection);
    }

    if (old_cli!=0) gles_glClientActiveTexture(GL_TEXTURE0+old_cli);

}

const char _blit_vsh[] = "#version 100                  \n\t" \
"attribute highp vec2 aPosition;                        \n\t" \
"attribute highp vec2 aTexCoord;                        \n\t" \
"varying mediump vec2 vTexCoord;                        \n\t" \
"void main(){                                           \n\t" \
"gl_Position = vec4(aPosition.x, aPosition.y, 0.0, 1.0);\n\t" \
"vTexCoord = aTexCoord;                                 \n\t" \
"}                                                      \n\t";

const char _blit_fsh[] = "#version 100                  \n\t" \
"uniform sampler2D uTex;                                \n\t" \
"varying mediump vec2 vTexCoord;                        \n\t" \
"void main(){                                           \n\t" \
"gl_FragColor = texture2D(uTex, vTexCoord);             \n\t" \
"}                                                      \n\t";

const char _blit_vsh_alpha[] = "#version 100            \n\t" \
"attribute highp vec2 aPosition;                        \n\t" \
"attribute highp vec2 aTexCoord;                        \n\t" \
"attribute lowp vec4 aColor;                            \n\t" \
"varying mediump vec2 vTexCoord;                        \n\t" \
"varying lowp vec4 vColor;                              \n\t" \
"void main(){                                           \n\t" \
"gl_Position = vec4(aPosition.x, aPosition.y, 0.0, 1.0);\n\t" \
"vTexCoord = aTexCoord;                                 \n\t" \
"vColor = aColor;                                       \n\t" \
"}                                                      \n\t";

const char _blit_fsh_alpha[] = "#version 100            \n\t" \
"uniform sampler2D uTex;                                \n\t" \
"varying mediump vec2 vTexCoord;                        \n\t" \
"varying lowp vec4 vColor;                              \n\t" \
"void main(){                                           \n\t" \
"lowp vec4 p = texture2D(uTex, vTexCoord);              \n\t" \
"if (p.a>0.0) discard;                                  \n\t" \
"gl_FragColor = p*vColor;                               \n\t" \
"}                                                      \n\t";

void gl4es_blitTexture_gles2(GLuint texture, 
    float width, float height, 
    float nwidth, float nheight, 
    float zoomx, float zoomy, 
    float vpwidth, float vpheight, 
    float x, float y, int mode) {

    LOAD_GLES(glDrawArrays);

    if(!glstate->blit) {
        LOAD_GLES2(glCreateShader);
        LOAD_GLES2(glShaderSource);
        LOAD_GLES2(glCompileShader);
        LOAD_GLES2(glGetShaderiv);
        LOAD_GLES2(glBindAttribLocation);
        LOAD_GLES2(glAttachShader);
        LOAD_GLES2(glCreateProgram);
        LOAD_GLES2(glLinkProgram);
        LOAD_GLES2(glGetProgramiv);
        LOAD_GLES(glGetUniformLocation);
        LOAD_GLES2(glUniform1i);

        glstate->blit = (glesblit_t*)malloc(sizeof(glesblit_t));
        memset(glstate->blit, 0, sizeof(glesblit_t));

        GLint success;
        const char *src[1];
        src[0] = _blit_fsh;
        glstate->blit->pixelshader = gles_glCreateShader( GL_FRAGMENT_SHADER );
        gles_glShaderSource( glstate->blit->pixelshader, 1, (const char**) src, NULL );
        gles_glCompileShader( glstate->blit->pixelshader );
        gles_glGetShaderiv( glstate->blit->pixelshader, GL_COMPILE_STATUS, &success );
        if (!success)
        {
            LOAD_GLES(glGetShaderInfoLog);
            char log[400];
            gles_glGetShaderInfoLog(glstate->blit->pixelshader_alpha, 399, NULL, log);
            SHUT(printf("LIBGL: Failed to produce blit fragment shader.\n%s", log);)
            free(glstate->blit);
            glstate->blit = NULL;
        }
    
        src[0] = _blit_fsh_alpha;
        glstate->blit->pixelshader_alpha = gles_glCreateShader( GL_FRAGMENT_SHADER );
        gles_glShaderSource( glstate->blit->pixelshader_alpha, 1, (const char**) src, NULL );
        gles_glCompileShader( glstate->blit->pixelshader_alpha );
        gles_glGetShaderiv( glstate->blit->pixelshader_alpha, GL_COMPILE_STATUS, &success );
        if (!success)
        {
            LOAD_GLES(glGetShaderInfoLog);
            char log[400];
            gles_glGetShaderInfoLog(glstate->blit->pixelshader_alpha, 399, NULL, log);
            SHUT(printf("LIBGL: Failed to produce blit with alpha fragment shader.\n%s", log);)
            free(glstate->blit);
            glstate->blit = NULL;
        }
    
        src[0] = _blit_vsh;
        glstate->blit->vertexshader = gles_glCreateShader( GL_VERTEX_SHADER );
        gles_glShaderSource( glstate->blit->vertexshader, 1, (const char**) src, NULL );
        gles_glCompileShader( glstate->blit->vertexshader );
        gles_glGetShaderiv( glstate->blit->vertexshader, GL_COMPILE_STATUS, &success );
        if( !success )
        {
            LOAD_GLES(glGetShaderInfoLog);
            char log[400];
            gles_glGetShaderInfoLog(glstate->blit->pixelshader_alpha, 399, NULL, log);
            SHUT(printf("LIBGL: Failed to produce blit vertex shader.\n%s", log);)
            free(glstate->blit);
            glstate->blit = NULL;
        }
    
        src[0] = _blit_vsh_alpha;
        glstate->blit->vertexshader_alpha = gles_glCreateShader( GL_VERTEX_SHADER );
        gles_glShaderSource( glstate->blit->vertexshader_alpha, 1, (const char**) src, NULL );
        gles_glCompileShader( glstate->blit->vertexshader_alpha );
        gles_glGetShaderiv( glstate->blit->vertexshader_alpha, GL_COMPILE_STATUS, &success );
        if( !success )
        {
            LOAD_GLES(glGetShaderInfoLog);
            char log[400];
            gles_glGetShaderInfoLog(glstate->blit->pixelshader_alpha, 399, NULL, log);
            SHUT(printf("LIBGL: Failed to produce blit with alpha vertex shader.\n%s", log);)
            free(glstate->blit);
            glstate->blit = NULL;
        }

        glstate->blit->program = gles_glCreateProgram();
        gles_glBindAttribLocation( glstate->blit->program, 0, "aPosition" );
        gles_glBindAttribLocation( glstate->blit->program, 1, "aTexCoord" );
        gles_glAttachShader( glstate->blit->program, glstate->blit->pixelshader );
        gles_glAttachShader( glstate->blit->program, glstate->blit->vertexshader );
        gles_glLinkProgram( glstate->blit->program );
        gles_glGetProgramiv( glstate->blit->program, GL_LINK_STATUS, &success );
        if( !success )
        {
            SHUT(printf("LIBGL: Failed to link blit program.\n");)
            free(glstate->blit);
            glstate->blit = NULL;
        }
        gles_glUniform1i( gles_glGetUniformLocation( glstate->blit->program, "uTex" ), 0 );

        glstate->blit->program_alpha = gles_glCreateProgram();
        gles_glBindAttribLocation( glstate->blit->program_alpha, 0, "aPosition" );
        gles_glBindAttribLocation( glstate->blit->program_alpha, 1, "aTexCoord" );
        gles_glBindAttribLocation( glstate->blit->program_alpha, 2, "aColor" );
        gles_glAttachShader( glstate->blit->program_alpha, glstate->blit->pixelshader_alpha );
        gles_glAttachShader( glstate->blit->program_alpha, glstate->blit->vertexshader_alpha );
        gles_glLinkProgram( glstate->blit->program_alpha );
        gles_glGetProgramiv( glstate->blit->program_alpha, GL_LINK_STATUS, &success );
        if( !success )
        {
            SHUT(printf("LIBGL: Failed to link blit program.\n");)
            free(glstate->blit);
            glstate->blit = NULL;
        }
        gles_glUniform1i( gles_glGetUniformLocation( glstate->blit->program_alpha, "uTex" ), 0 );
    }

    int customvp = (vpwidth>0.0);
    float w2 = 2.0f / (customvp?vpwidth:glstate->raster.viewport.width);
    float h2 = 2.0f / (customvp?vpheight:glstate->raster.viewport.height);
    float blit_x1=x;
    float blit_x2=x+width*zoomx;
    float blit_y1=y;
    float blit_y2=y+height*zoomy;
    GLfloat *vert = glstate->blit->vert;
    GLfloat *tex = glstate->blit->tex;
    vert[0] = blit_x1*w2-1.0f;  vert[1] = blit_y1*h2-1.0f;
    vert[2] = blit_x2*w2-1.0f;  vert[3] = vert[1];
    vert[4] = vert[2];          vert[5] = blit_y2*h2-1.0f;
    vert[6] = vert[0];          vert[7] = vert[5];
    GLfloat rw = width/nwidth;
    GLfloat rh = height/nheight;
    tex[0] = 0.0f;  tex[1] = 0.0f;
    tex[2] = rw;    tex[3] = 0.0f;
    tex[4] = rw;    tex[5] = rh;
    tex[6] = 0.0f;  tex[7] = rh;

    int alpha = 0;
    switch (mode) {
        case BLIT_OPAQUE:
            //gl4es_glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            break;
        case BLIT_ALPHA:
            alpha = 1;
            break;
        case BLIT_COLOR:
            //gl4es_glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            break;
    }

    realize_blitenv(alpha);

    gles_glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void gl4es_blitTexture(GLuint texture, 
    float width, float height, 
    float nwidth, float nheight, 
    float zoomx, float zoomy, 
    float vpwidth, float vpheight, 
    float x, float y, int mode) {
//printf("blitTexture(%d, %f, %f, %f, %f, %f, %f, %f, %f, %d) customvp=%d, vp=%d/%d/%d/%d\n", texture, width, height, nwidth, nheight, vpwidth, vpheight, x, y, mode, (vpwidth>0.0), glstate->raster.viewport.x, glstate->raster.viewport.y, glstate->raster.viewport.width, glstate->raster.viewport.height);
    LOAD_GLES(glBindTexture);
    LOAD_GLES(glActiveTexture);
    gl4es_glPushAttrib(GL_TEXTURE_BIT | GL_ENABLE_BIT | GL_TRANSFORM_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT);

    GLuint old_tex = glstate->texture.active;
    if (old_tex!=0) gles_glActiveTexture(GL_TEXTURE0);

    gl4es_glDisable(GL_DEPTH_TEST);
    gl4es_glDisable(GL_CULL_FACE);

    gltexture_t *old_bind = glstate->texture.bound[0][ENABLED_TEX2D];
    gl4es_glEnable(GL_TEXTURE_2D);
    gles_glBindTexture(GL_TEXTURE_2D, texture);

    if(hardext.esversion==1) {
        gl4es_blitTexture_gles1(texture, width, height, 
                                nwidth, nheight, zoomx, zoomy, 
                                vpwidth, vpheight, x, y, mode);
    } else {
        gl4es_blitTexture_gles2(texture, width, height, 
            nwidth, nheight, zoomx, zoomy, 
            vpwidth, vpheight, x, y, mode);
    }

    // All the previous states are Pushed / Poped anyway...
    if (old_bind == NULL) 
        gles_glBindTexture(GL_TEXTURE_2D, 0);
    else
        gles_glBindTexture(GL_TEXTURE_2D, old_bind->glname);

    if (old_tex!=0) gles_glActiveTexture(GL_TEXTURE0+old_tex);
    
    gl4es_glPopAttrib();
}