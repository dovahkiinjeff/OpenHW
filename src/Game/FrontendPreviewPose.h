#ifndef HW_FRONTEND_PREVIEW_POSE_H
#define HW_FRONTEND_PREVIEW_POSE_H
/* Menu-view transform only, in OpenGL column order. Engine model coordinates
   are X up, Y right, Z forward. Put forward along the screen's horizontal X
   axis and up along its vertical Y axis; retain a right-handed basis. */
static const float hwFrontendPreviewBasis[16]={
    0,1,0,0,
    0,0,1,0,
    1,0,0,0,
    0,0,0,1
};
#endif
