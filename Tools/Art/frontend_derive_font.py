"""Create OFL-compliant JTS UI Sans derivatives with project-specific ASCII metrics."""
import json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Saved/FrontEndPython'))
from fontTools.ttLib import TTFont
OUT=ROOT/'SourceArt/FrontEnd'
(OUT/'Fonts').mkdir(exist_ok=True)
entries=[]
for weight in ['Regular','Bold']:
    font=TTFont(OUT/'External'/('SourceHanSansSC-'+weight+'.otf'))
    family='JTS UI Sans';ps='JTSUISans-'+weight
    names={1:family,2:weight,3:'JTS UI Sans 1.000; derived from 2.005R',4:family+' '+weight,6:ps,16:family,17:weight}
    for name_id,value in names.items():
        for record in list(font['name'].names):
            if record.nameID==name_id:font['name'].setName(value,name_id,record.platformID,record.platEncID,record.langID)
    cff=font['CFF '].cff;cff.fontNames=[ps]
    top=cff.topDictIndex[0];top.FamilyName=family;top.FullName=family+' '+weight
    cmap=font.getBestCmap();metrics=font['hmtx'].metrics
    # Tabular digits and slightly roomier ASCII punctuation for codes and small UI labels.
    digit_width=round(font['head'].unitsPerEm*.58)
    for char in '0123456789':
        glyph=cmap[ord(char)];width,lsb=metrics[glyph];metrics[glyph]=(digit_width,lsb+(digit_width-width)//2)
    for char in ':;.,!?()[]':
        glyph=cmap[ord(char)];width,lsb=metrics[glyph];metrics[glyph]=(width+24,lsb+12)
    path=OUT/'Fonts'/(ps+'.otf');font.save(path)
    entries.append({'id':ps,'source':'adobe_ofl_derivative','file':'Fonts/'+path.name,'glyphs':len(font.getGlyphOrder()),'adaptations':['Renamed modified family and PostScript names; reserved Source name removed','Tabular 0-9 metrics at 580/1000 em','ASCII punctuation +24/1000 em spacing','Chinese coverage retained without subsetting']})
(OUT/'Fonts/OFL.txt').write_bytes((OUT/'External/OFL_SourceHanSans.txt').read_bytes())
(OUT/'Fonts/MODIFICATIONS.txt').write_text('JTS UI Sans derived from Adobe Source Han Sans SC 2.005R. Original copyright and OFL in OFL.txt. Font outlines retained. Family renamed and ASCII metrics adapted for Jump to Space.\n',encoding='utf-8')
(OUT/'fonts_manifest.json').write_text(json.dumps(entries,indent=2),encoding='utf-8')
print(entries)
