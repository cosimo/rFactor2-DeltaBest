#if 1

    #define CUSTOMFVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)
    struct CUSTOMVERTEX {
        FLOAT X, Y, Z, RHW;
        DWORD COLOR;
    };
    CUSTOMVERTEX vertices[] = {
        { -10.0f,  10.0f, 0.0f, D3DCOLOR_XRGB(0, 0, 255), },
        {  10.0f,  10.0f, 0.0f, D3DCOLOR_XRGB(0, 255, 0), },
        { -10.0f, -10.0f, 0.0f, D3DCOLOR_XRGB(255, 0, 0), },
        {  10.0f, -10.0f, 0.0f, D3DCOLOR_XRGB(0, 255, 255), },
    }; 

    // Create a vertex buffer
    LPDIRECT3DDEVICE9 d3d = (LPDIRECT3DDEVICE9) info.mDevice;
    IDirect3DVertexBuffer9* v_buffer;
    VOID* p;
    d3d->CreateVertexBuffer(4 * sizeof(CUSTOMVERTEX),    // change to 4, instead of 3
        0, CUSTOMFVF, D3DPOOL_MANAGED, &v_buffer, NULL);

    // Lock v_buffer and load the vertices into it
    v_buffer->Lock(0, 0, (void**) &p, 0);
    memcpy(p, vertices, sizeof(vertices));
    v_buffer->Unlock();

    d3d->SetStreamSource(0, v_buffer, 0, sizeof(CUSTOMVERTEX));
    d3d->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
#endif
