/* Original UI call sites, including 005209f0, cmClose 00455680,
   mrCycleSelectionTactics 00566860 and Sensors/manager entry points.
   Cataclysm's UI event numbering is not HW1's event numbering. */
static sdword cataUiEventRow(sdword event)
{
    static const sdword rows[]={0,4,1,6,2,3,8,10,9,22,23,24,33,34,
        37,43,35,36,39,38,40,41,8,44,45,46,47,48,49,50,51,52,53,54};
    sdword index=event&SFX_Event_Mask;
    return index<(sdword)(sizeof(rows)/sizeof(rows[0]))?rows[index]:-1;
}
