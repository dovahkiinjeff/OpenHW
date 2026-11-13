/* A localized classic speech bank must travel with its own matching lookup
   tables. Never substitute a VCE against the English sentence offsets. */
static void speechLanguagePaths(char *sentence, char *phrase, char *voice)
{
    static const char *names[]={"English","French","German","Spanish","Italian"};
    char paths[3][100];
    udword checksum[3],header[2];
    sdword i;
    if(strVoiceLanguage==0 || strVoiceLanguage>=5) return;
    sprintf(paths[0],"Localization\\%s\\SoundFX\\speechsentence_comp.lut",names[strVoiceLanguage]);
    sprintf(paths[1],"Localization\\%s\\SoundFX\\speechphrase_comp.lut",names[strVoiceLanguage]);
    sprintf(paths[2],"Localization\\%s\\HW_Comp.vce",names[strVoiceLanguage]);
    for(i=0;i<3;++i) {
        filehandle handle;
        if(!fileExists(paths[i],0) || fileSizeGet(paths[i],0)<16) return;
        handle=fileOpen(paths[i],FF_ReturnNULLOnFail);
        if(!handle) return;
        if(fileBlockReadNoError(handle,header,sizeof(header))!=sizeof(header)) {fileClose(handle);return;}
        fileClose(handle);
        checksum[i]=header[1];
    }
    if(checksum[0]!=checksum[1] || checksum[0]!=checksum[2]) return;
    strcpy(sentence,paths[0]);strcpy(phrase,paths[1]);strcpy(voice,paths[2]);
}
