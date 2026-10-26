/* Private legacy lighting stack. Model hierarchy depth is not bounded to ten. */
#ifndef HW_SHADER_LIGHT_MATRIX_STACK_H
#define HW_SHADER_LIGHT_MATRIX_STACK_H

static hmatrix *shLightMatStack;
static sdword shLightMatStackPos;
static sdword shLightMatStackCapacity;
static void shLightMatrixStackReset(void);
static void shLightMatrixStackRelease(void);

#endif
