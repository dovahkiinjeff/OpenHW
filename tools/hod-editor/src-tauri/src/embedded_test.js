if (document.querySelector(".toolbar") && !window.testingWorkshop) {
  window.testingWorkshop=true;
  (async()=>{
    const until=async(check,label)=>{
      for(let i=0;i<220;i++){if(check())return;await new Promise(r=>setTimeout(r,100));}
      throw Error(label+" | "+document.body.innerText.slice(-450));
    };
    const button=text=>Array.from(document.querySelectorAll("button")).find(b=>b.textContent.trim()===text);
    const click=async text=>{await until(()=>button(text),text);button(text).click();};
    window.alert=()=>{};window.confirm=()=>true;
    await click("Game models");await click("r1/lightinterceptor/rmnative/hull.hod");
    await until(()=>!button("Quick Add subsystem").disabled,"load ship");
    await click("Quick Add subsystem");await click("kus_assaultgun_hw1_4fe150a31d");
    await until(()=>document.body.innerText.includes("OWMount_1"),"add mount");
    await until(()=>!button("Quick Add subsystem").disabled,"finish adding mount");
    await new Promise(r=>setTimeout(r,200));
    window.dispatchEvent(new KeyboardEvent("keydown",{key:"c",ctrlKey:true,bubbles:true}));
    await until(()=>document.body.innerText.includes("Copied kus_"),"copy mount");
    window.dispatchEvent(new KeyboardEvent("keydown",{key:"v",ctrlKey:true,bubbles:true}));
    await until(()=>document.body.innerText.includes("OWMount_2"),"copy and paste");
    await click("Save As...");
    await until(()=>document.body.innerText.includes("compiled successfully"),"save HOD and definition");
    await click("New HOD");await until(()=>document.body.innerText.includes("New HOD model successfully initialized"),"new model");
    await click("Open");await until(()=>document.body.innerText.includes("OWMount_2"),"reopen saved ship");
    await click("Quick Add subsystem");
    await click("Duplicate OWMount_2 · kus_assaultgun_hw1_4fe150a31d");
    await until(()=>document.body.innerText.includes("OWMount_3"),"duplicate reopened mount");
    await click("Save HOD");await until(()=>document.body.innerText.includes("compiled successfully"),"resave");
    await click("Return to game");
  })().catch(e=>window.ipc.postMessage(JSON.stringify({id:0,command:"qa_error",args:{text:e.message}})));
}
