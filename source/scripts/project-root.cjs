'use strict';
const fs=require('fs'),path=require('path');
let candidate=__dirname;
while(true){
 if(fs.existsSync(path.join(candidate,'.project-layout.json'))){module.exports=candidate;break;}
 const parent=path.dirname(candidate);
 if(parent===candidate)throw Error('No se encontro la raiz del proyecto PES 6.');
 candidate=parent;
}
