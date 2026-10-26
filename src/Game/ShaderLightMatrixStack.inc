static void shMultiplyHMatByHMatRot(hmatrix *result, hmatrix *first, hmatrix *second)
{
	result->m11 = matrixdot(first->m11,first->m12,first->m13,second->m11,second->m21,second->m31);
	result->m12 = matrixdot(first->m11,first->m12,first->m13,second->m12,second->m22,second->m32);
	result->m13 = matrixdot(first->m11,first->m12,first->m13,second->m13,second->m23,second->m33);

	result->m21 = matrixdot(first->m21,first->m22,first->m23,second->m11,second->m21,second->m31);
	result->m22 = matrixdot(first->m21,first->m22,first->m23,second->m12,second->m22,second->m32);
	result->m23 = matrixdot(first->m21,first->m22,first->m23,second->m13,second->m23,second->m33);

	result->m31 = matrixdot(first->m31,first->m32,first->m33,second->m11,second->m21,second->m31);
	result->m32 = matrixdot(first->m31,first->m32,first->m33,second->m12,second->m22,second->m32);
	result->m33 = matrixdot(first->m31,first->m32,first->m33,second->m13,second->m23,second->m33);
}


/* Grow before taking parent/child pointers: memRealloc may move the stack.
   Retain the high-water capacity between draws, so steady rendering allocates
   nothing. Every new entry starts with the same homogeneous identity basis. */
static void shLightMatrixStackReserve(sdword needed)
{
    sdword capacity = shLightMatStackCapacity;
    if (needed <= capacity) return;
    if (capacity == 0) capacity = 16;
    while (capacity < needed)
    {
        if (capacity > (SDWORD_Max / (sdword)sizeof(hmatrix)) / 2)
            dbgFatalf(DBG_Loc, "Lighting matrix hierarchy is too large");
        capacity *= 2;
    }
    if (shLightMatStack == NULL)
    {
        shLightMatStack = memAlloc(capacity * sizeof(hmatrix), "light matrix stack", NonVolatile);
    }
    else
    {
        shLightMatStack = memRealloc(shLightMatStack, capacity * sizeof(hmatrix), "light matrix stack", NonVolatile);
    }
    shLightMatStackCapacity = capacity;
}

static void shLightMatrixStackReset(void)
{
    shLightMatrixStackReserve(1);
    shLightMatStackPos = 0;
    shLightMatStack[0] = IdentityHMatrix;
}

static void shLightMatrixStackRelease(void)
{
    if (shLightMatStack != NULL) memFree(shLightMatStack);
    shLightMatStack = NULL;
    shLightMatStackCapacity = 0;
    shLightMatStackPos = 0;
}

void shPushLightMatrix(hmatrix *pMat)
{
    hmatrix *parent, *child;
    shLightMatrixStackReserve(shLightMatStackPos + 2);
    parent = &shLightMatStack[shLightMatStackPos];
    child = &shLightMatStack[++shLightMatStackPos];
    *child = IdentityHMatrix;
    shMultiplyHMatByHMatRot(child, parent, pMat);
    shUpdateLightsFromMatrix(child);
}

void shPopLightMatrix(void)
{
    /* A mismatched pop must not turn the next push into a write before the
       allocation. Balanced callers still restore the exact parent matrix. */
    if (shLightMatStackPos == 0)
    {
        dbgAssertOrIgnore(shLightMatStackPos > 0);
        return;
    }
    --shLightMatStackPos;
    shUpdateLightsFromMatrix(&shLightMatStack[shLightMatStackPos]);
}
