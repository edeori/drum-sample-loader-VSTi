const instruments = [
  { id: 1, name: 'Snare Drum', note: 'C1', category: 'Body / Snare', type: 'snare', muted: false, solo: false },
  { id: 2, name: 'Kick Drum', note: 'C0', category: 'Body / Kick', type: 'kick', muted: false, solo: false },
  { id: 3, name: 'Hi-Hat', note: 'F#1', category: 'Cymbal', type: 'cymbal', muted: false, solo: false, articulations: [
    { name: 'Closed', note: 'F#1' }, { name: 'Half Open', note: 'G#1' }, { name: 'Open', note: 'A#1' }
  ] },
  { id: 4, name: 'Tom 1', note: 'D1', category: 'Body / Tom', type: 'tom', muted: false, solo: false },
  { id: 5, name: 'Tom 2', note: 'E1', category: 'Body / Tom', type: 'tom', muted: false, solo: false },
  { id: 6, name: 'Floor Tom', note: 'A1', category: 'Body / Tom', type: 'tom', muted: false, solo: false },
  { id: 7, name: 'Crash L', note: 'C#2', category: 'Cymbal', type: 'cymbal', muted: false, solo: false, articulations: [
    { name: 'Hit', note: 'C#2' }, { name: 'Choke', note: 'C#3' }
  ] },
  { id: 8, name: 'Crash R', note: 'A2', category: 'Cymbal', type: 'cymbal', muted: false, solo: false, articulations: [
    { name: 'Hit', note: 'A2' }, { name: 'Choke', note: 'A#2' }
  ] },
  { id: 9, name: 'Ride', note: 'D#2', category: 'Cymbal', type: 'cymbal', muted: false, solo: false },
  { id: 10, name: 'China', note: 'E2', category: 'Cymbal', type: 'cymbal', muted: false, solo: false }
];

const state = {
  selectedId: 1,
  layer: 2,
  thresholds: [31, 63, 95],
  layerCount: 4,
  meterTimer: null,
  pendingCell: null,
  slotCount: 4,
  articulation: 0,
  overheadFormat: 'split',
  roomFormat: 'split'
};

const $ = (selector, scope = document) => scope.querySelector(selector);
const $$ = (selector, scope = document) => [...scope.querySelectorAll(selector)];

const refs = {
  grid: $('#instrumentGrid'), search: $('#searchInput'), count: $('#instrumentCount'),
  editorName: $('#editorName'), editorCategory: $('#editorCategory'), midiNote: $('#midiNote'),
  articulation: $('#articulationSelect'), articulationField: $('#articulationField'),
  layerTabs: $('#layerTabs'), sampleTable: $('#sampleTable'), mixer: $('#mixerChannels'),
  midiList: $('#midiList'), thresholdControls: $('#thresholdControls'), velocityStrip: $('#velocityStrip')
};

const layerNames = ['Ghost', 'Soft', 'Medium', 'Heavy', 'Extreme'];

function currentInstrument() { return instruments.find(i => i.id === state.selectedId) || instruments[0]; }
function getArticulations(inst = currentInstrument()) { return inst.articulations || [{ name: 'Main', note: inst.note }]; }
function currentArticulation() { return getArticulations()[state.articulation] || getArticulations()[0]; }

function getMicChannels(inst = currentInstrument()) {
  const spotChannels = {
    snare: ['Top (spot)', 'Bottom (spot)'],
    kick: ['Kick In (spot)', 'Kick Out (spot)'],
    tom: ['Spot'],
    cymbal: ['Spot']
  }[inst.type] || ['Spot'];
  const overheads = state.overheadFormat === 'stereo' ? ['OH (Stereo)'] : ['OH L', 'OH R'];
  const rooms = state.roomFormat === 'stereo' ? ['Room (Stereo)'] : ['Room L', 'Room R'];
  return [...spotChannels, ...overheads, ...rooms];
}

function renderCards(filter = '') {
  const query = filter.trim().toLowerCase();
  const visible = instruments.filter(i => `${i.name} ${i.note} ${i.category} ${getArticulations(i).map(a => `${a.name} ${a.note}`).join(' ')}`.toLowerCase().includes(query));
  refs.grid.innerHTML = visible.map(i => `
    <article class="instrument-card ${i.id === state.selectedId ? 'selected' : ''}" data-id="${i.id}" tabindex="0" role="button" aria-label="${i.name} kiválasztása">
      <h3>${i.name}</h3><div class="note">${i.articulations ? `${i.articulations.length} articulations` : i.note}</div>
      <img class="drum-art ${i.type}" src="assets/instruments/${i.type}.png" alt="">
      <div class="card-controls"><button class="toggle-button mute ${i.muted ? 'on' : ''}" data-action="mute" aria-label="Mute">M</button><button class="toggle-button ${i.solo ? 'on' : ''}" data-action="solo" aria-label="Solo">S</button></div>
    </article>`).join('') + (query ? '' : `<button class="instrument-card empty-card" id="emptyAdd"><div><b>＋</b><span>ADD INSTRUMENT</span></div></button>`);
  refs.count.textContent = `${instruments.length} instruments`;
}

function renderEditor() {
  const inst = currentInstrument();
  const articulations = getArticulations(inst);
  state.articulation = Math.min(state.articulation, articulations.length - 1);
  refs.editorName.textContent = inst.name;
  refs.editorCategory.textContent = `${inst.category}${articulations.length > 1 ? ` · ${articulations.length} articulations` : ''}`;
  $('#editorIcon').src = `assets/instruments/${inst.type}.png`;
  refs.articulation.innerHTML = articulations.map((a,index) => `<option value="${index}">${a.name}</option>`).join('');
  refs.articulation.value = String(state.articulation);
  refs.articulationField.classList.toggle('hidden', articulations.length === 1);
  refs.midiNote.value = articulations[state.articulation].note;
  renderLayers();
  renderSamples();
  renderMixer();
  renderMidi();
}

function noteOptions() {
  const notes = ['C0','C#0','D0','D#0','E0','F0','F#0','G0','G#0','A0','A#0','B0','C1','C#1','D1','D#1','E1','F1','F#1','G1','G#1','A1','A#1','B1','C2','C#2','D2','D#2','E2','F2','F#2','G2','G#2','A2','A#2','B2','C3','C#3','D3'];
  refs.midiNote.innerHTML = notes.map(n => `<option>${n}</option>`).join('');
}

function getRanges() {
  const bounds = [...state.thresholds, 127];
  let start = 0;
  return bounds.map(end => { const range = [start, end]; start = end + 1; return range; });
}

function renderLayers() {
  const ranges = getRanges();
  refs.layerTabs.style.gridTemplateColumns = `repeat(${state.layerCount},1fr)`;
  refs.layerTabs.innerHTML = ranges.map((r, index) => `<button class="layer-tab ${state.layer === index ? 'active' : ''}" data-layer="${index}">${layerNames[index]} <small>(${r[0]}–${r[1]})</small></button>`).join('');
}

function sampleName(inst, mic, slot) {
  const prefix = inst.name.toLowerCase().replace(/[^a-z0-9]/g, '').slice(0, 4);
  const micCode = mic.toLowerCase().replace(/[()]/g, '').replace(/spot/g, '').trim().replace(/\s+/g, '_');
  const layerCode = ['g','s','m','h','x'][state.layer];
  const articulationCode = currentArticulation().name.toLowerCase().replace(/[^a-z0-9]/g, '').slice(0, 4);
  if ((slot + mic.length + state.layer) % 7 === 0) return '';
  return `${prefix}_${articulationCode}_${micCode}_${layerCode}${slot}.wav`;
}

function renderSamples() {
  const inst = currentInstrument();
  const mics = getMicChannels(inst);
  let head = '<thead><tr><th>MIC CHANNELS</th>';
  for (let s = 1; s <= state.slotCount; s++) head += `<th>Slot ${s}</th>`;
  head += '<th><button class="add-slot" id="addSlot">＋ ADD SLOT</button></th></tr></thead>';
  const rows = mics.map(mic => `<tr><td>♩&nbsp;&nbsp;${mic}</td>${Array.from({length:state.slotCount},(_,s) => {
    const file = sampleName(inst,mic,s+1);
    return `<td class="sample-cell ${file ? '' : 'empty'}" data-mic="${mic}" data-slot="${s+1}"><div class="sample-file">${file ? `<span class="wave-icon">▮▯▮▯</span><span>${file}</span>` : '<span>＋</span><span>Load sample</span>'}</div></td>`;
  }).join('')}<td class="sample-cell empty"><div class="sample-file"><span>—</span></div></td></tr>`).join('');
  refs.sampleTable.innerHTML = `${head}<tbody>${rows}</tbody>`;
}

function renderMixer() {
  const mics = getMicChannels();
  const channels = [...mics, 'MASTER'];
  refs.mixer.style.gridTemplateColumns = `repeat(${channels.length},1fr)`;
  refs.mixer.innerHTML = channels.map((mic,index) => index === channels.length - 1 ? `
    <div class="mixer-channel"><h4>MASTER</h4><div class="master-mini-meter"><i></i><i></i></div><span class="fader-value">0.0 dB</span><span class="route-label">OUTPUT</span><select class="route-select"><option>Master</option></select></div>` : `
    <div class="mixer-channel"><h4>${mic}</h4><div class="fader-wrap"><input class="vertical-fader" data-fader data-default="0" type="range" min="-60" max="6" step="0.1" value="0" aria-label="${mic} level"></div><output class="fader-value">0.0 dB</output>${renderPan(mic)}<span class="route-label">ROUTE</span><select class="route-select"><option>${mic.includes('OH') ? 'Overheads' : mic.includes('Room') ? 'Room' : `Out ${index*2+1}/${index*2+2}`}</option><option>Master</option><option>Out 1/2</option><option>Out 3/4</option><option>Out 5/6</option></select></div>`).join('');
}

function renderPan(mic) {
  const isStereo = mic.includes('(Stereo)');
  const pan = isStereo ? 'STEREO' : mic.endsWith(' L') ? 'L100' : mic.endsWith(' R') ? 'R100' : 'C';
  const panClass = isStereo ? 'stereo' : mic.endsWith(' L') ? 'left' : mic.endsWith(' R') ? 'right' : '';
  return `<div class="channel-pan"><span>PAN</span><b class="${panClass}">${pan}</b></div>`;
}

function renderMidi() {
  refs.midiList.innerHTML = instruments.flatMap(i => getArticulations(i).map(a => ({ ...a, instrument: i }))).map(a => `<div class="midi-row"><span class="note-badge">${a.note}</span><span>${a.instrument.name}${a.name === 'Main' ? '' : ` · ${a.name}`}</span><em>${a.instrument.type === 'cymbal' ? 'CYM' : 'BODY'}</em></div>`).join('');
}

function normalizeThresholds(count) {
  const defaults = { 3:[42,84], 4:[31,63,95], 5:[25,51,76,102] };
  state.layerCount = count;
  state.thresholds = defaults[count];
  state.layer = Math.min(state.layer,count-1);
  renderThresholds(); renderLayers(); renderSamples();
}

function renderThresholds() {
  const ranges = getRanges();
  refs.velocityStrip.innerHTML = ranges.map((r,i) => `<span class="velocity-segment" style="flex:${r[1]-r[0]+1}" data-range="${r[0]}–${r[1]}" title="${layerNames[i]}: ${r[0]}–${r[1]}"></span>`).join('');
  refs.thresholdControls.innerHTML = state.thresholds.map((value,i) => `<div class="threshold-row"><label>THRESHOLD ${i+1}</label><output>${value}</output><input type="range" min="${i ? state.thresholds[i-1]+1 : 1}" max="${i < state.thresholds.length-1 ? state.thresholds[i+1]-1 : 126}" value="${value}" data-threshold="${i}"></div>`).join('');
}

function toast(title, text = '') {
  const el = document.createElement('div'); el.className = 'toast';
  el.innerHTML = `<strong>${title}</strong>${text ? `<span>${text}</span>` : ''}`;
  $('#toastRegion').append(el);
  setTimeout(() => { el.style.opacity = '0'; el.style.transform = 'translateY(6px)'; setTimeout(() => el.remove(),200); }, 2500);
}

function showInfo(eyebrow,title,text) {
  $('#infoEyebrow').textContent = eyebrow; $('#infoTitle').textContent = title; $('#infoText').textContent = text; $('#infoDialog').showModal();
}

function playPreview(label = 'Preview') {
  clearInterval(state.meterTimer);
  toast(label, `${currentInstrument().name} · ${currentArticulation().name} · ${layerNames[state.layer]} layer`);
  let ticks = 0;
  state.meterTimer = setInterval(() => {
    const decay = Math.max(0, 1 - ticks / 24);
    const l = Math.round((28 + Math.random()*68)*decay), r = Math.round((24 + Math.random()*72)*decay);
    $('#meterL').style.height = `${l}%`; $('#meterR').style.height = `${r}%`;
    $('#peakL').textContent = `${(-18 + l*.17).toFixed(1)}`; $('#peakR').textContent = `${(-18 + r*.17).toFixed(1)}`;
    $$('.master-mini-meter i').forEach((bar,i) => bar.style.clipPath = `inset(${100-(i ? r:l)}% 0 0 0)`);
    if (++ticks > 24) { clearInterval(state.meterTimer); $('#meterL').style.height = '0'; $('#meterR').style.height = '0'; }
  },80);
}

refs.grid.addEventListener('click', e => {
  const action = e.target.closest('[data-action]');
  const card = e.target.closest('.instrument-card');
  if (e.target.closest('#emptyAdd')) return $('#instrumentDialog').showModal();
  if (!card) return;
  const inst = instruments.find(i => i.id === Number(card.dataset.id));
  if (action) {
    e.stopPropagation(); const key = action.dataset.action === 'mute' ? 'muted' : 'solo'; inst[key] = !inst[key];
    renderCards(refs.search.value); toast(`${inst.name}: ${action.dataset.action.toUpperCase()} ${inst[key] ? 'ON' : 'OFF'}`); return;
  }
  state.selectedId = inst.id; state.articulation = 0; state.slotCount = 4; renderCards(refs.search.value); renderEditor();
});

refs.grid.addEventListener('keydown', e => { if ((e.key === 'Enter' || e.key === ' ') && e.target.matches('.instrument-card')) e.target.click(); });
refs.search.addEventListener('input', e => renderCards(e.target.value));
refs.articulation.addEventListener('change', e => { state.articulation = Number(e.target.value); renderEditor(); toast('Articulation selected', `${currentInstrument().name} · ${currentArticulation().name} · ${currentArticulation().note}`); });
refs.layerTabs.addEventListener('click', e => { const tab = e.target.closest('[data-layer]'); if (!tab) return; state.layer = Number(tab.dataset.layer); renderLayers(); renderSamples(); });
refs.sampleTable.addEventListener('click', e => {
  if (e.target.closest('#addSlot')) { state.slotCount++; renderSamples(); toast('Round-robin slot added', `${state.slotCount} slots in ${layerNames[state.layer]}`); return; }
  const cell = e.target.closest('.sample-cell[data-mic]'); if (!cell) return; state.pendingCell = cell; $('#sampleFile').click();
});
$('#sampleFile').addEventListener('change', e => { if (!e.target.files[0] || !state.pendingCell) return; state.pendingCell.classList.remove('empty'); state.pendingCell.innerHTML = `<div class="sample-file"><span class="wave-icon">▮▯▮▯</span><span>${e.target.files[0].name}</span></div>`; toast('Sample loaded', e.target.files[0].name); e.target.value = ''; });
refs.thresholdControls.addEventListener('input', e => { if (!e.target.matches('[data-threshold]')) return; state.thresholds[Number(e.target.dataset.threshold)] = Number(e.target.value); renderThresholds(); renderLayers(); });
$('#layerCount').addEventListener('change', e => normalizeThresholds(Number(e.target.value)));
$('#resetThresholds').addEventListener('click', () => { normalizeThresholds(Number($('#layerCount').value)); toast('Velocity thresholds reset'); });
refs.mixer.addEventListener('input', e => { if (!e.target.matches('.vertical-fader')) return; const output = e.target.closest('.mixer-channel').querySelector('.fader-value'); output.textContent = `${Number(e.target.value).toFixed(1)} dB`; const pct=(Number(e.target.value)+60)/66*100; e.target.style.background=`linear-gradient(90deg,#346fae ${pct}%,#070a0c ${pct}%)`; });
$$('.horizontal-control input').forEach(slider => slider.addEventListener('input', () => { const output=slider.parentElement.querySelector('output'); const unit=slider.id==='masterVolume'||slider.parentElement.textContent.includes('OVERHEAD')||slider.parentElement.textContent.includes('ROOM')?' dB':' st'; output.textContent=`${Number(slider.value).toFixed(unit===' st'?0:1)}${unit}`; if(slider.id==='masterVolume') $('#masterOutput').textContent=output.textContent; }));

function resetFader(slider) {
  const now = Date.now();
  if (now - Number(slider.dataset.lastReset || 0) < 150) return;
  slider.dataset.lastReset = String(now);
  slider.value = slider.dataset.default || '0';
  slider.dispatchEvent(new Event('input', { bubbles: true }));
  toast('Fader reset', '0.0 dB');
}

document.addEventListener('dblclick', e => { if (e.target.matches('[data-fader]')) { e.preventDefault(); resetFader(e.target); } });
document.addEventListener('click', e => { if ((e.metaKey || e.ctrlKey) && e.target.matches('[data-fader]')) { e.preventDefault(); resetFader(e.target); } });
document.addEventListener('contextmenu', e => { if (e.ctrlKey && e.target.matches('[data-fader]')) { e.preventDefault(); resetFader(e.target); } });
$('#overheadFormat').addEventListener('change', e => { state.overheadFormat = e.target.value; renderSamples(); renderMixer(); toast('Overhead format updated', e.target.value === 'stereo' ? 'Single stereo channel' : 'Separate L/R · hard panned'); });
$('#roomFormat').addEventListener('change', e => { state.roomFormat = e.target.value; renderSamples(); renderMixer(); toast('Room format updated', e.target.value === 'stereo' ? 'Single stereo channel' : 'Separate L/R · hard panned'); });

$('#previewAll').addEventListener('click', () => playPreview('Previewing all mic channels'));
$('#addInstrument').addEventListener('click', () => $('#instrumentDialog').showModal());
$('#confirmInstrument').addEventListener('click', e => {
  const form=$('#instrumentForm'); if(!form.reportValidity()){e.preventDefault();return}
  const category=$('input[name="category"]:checked').value; const name=$('#newInstrumentName').value.trim();
  const type=category==='Cymbal'?'cymbal':'tom'; const inst={id:Date.now(),name,note:$('#newMidiNote').value,category,type,muted:false,solo:false};
  instruments.push(inst);state.selectedId=inst.id;state.articulation=0;renderCards();renderEditor();toast('Instrument added',`${name} · ${inst.note}`);
});
$('#editName').addEventListener('click', () => { const inst=currentInstrument(); const name=prompt('Instrument name',inst.name); if(name?.trim()){inst.name=name.trim();renderCards(refs.search.value);renderEditor();toast('Instrument renamed');} });
$('#deleteInstrument').addEventListener('click', () => { if(instruments.length===1)return; const inst=currentInstrument(); if(confirm(`Remove ${inst.name} from the kit?`)){instruments.splice(instruments.indexOf(inst),1);state.selectedId=instruments[0].id;renderCards();renderEditor();toast('Instrument removed',inst.name);} });
refs.midiNote.addEventListener('change', () => { const inst=currentInstrument(); if(inst.articulations) inst.articulations[state.articulation].note=refs.midiNote.value; else inst.note=refs.midiNote.value; if(state.articulation===0) inst.note=refs.midiNote.value;renderCards(refs.search.value);renderMidi();toast('MIDI note updated',`${currentArticulation().name} · ${refs.midiNote.value}`); });
$('#midiLearn').addEventListener('click', e => { e.currentTarget.classList.toggle('learning');e.currentTarget.textContent=e.currentTarget.classList.contains('learning')?'PLAY A NOTE…':'MIDI LEARN'; if(e.currentTarget.classList.contains('learning'))toast('MIDI Learn active','Play a note on your controller'); });
$('#importKit').addEventListener('click', () => $('#kitFile').click());
$('#kitFile').addEventListener('change',e=>{if(e.target.files[0])toast('Kit imported',e.target.files[0].name)});
$('#saveButton').addEventListener('click',()=>toast('Preset saved','Modern Metal Kit.dvk'));
$('#settingsButton').addEventListener('click',()=>$('#sidePanel').scrollIntoView({behavior:'smooth'}));
$('#helpButton').addEventListener('click',()=>showInfo('HELP','Interactive Swarm mockup','Select an instrument, switch velocity layers, load samples into cells and shape the mix with the channel faders. Preview triggers the animated master meters.'));
$('#brandButton').addEventListener('click',()=>showInfo('SWARM v1.0','Built for real drum performances','A multi-channel drum sample instrument concept with velocity layers, non-repeating round robins, flexible routing and a let-it-ring playback model.'));
$('#instrumentSettings').addEventListener('click',()=>showInfo('INSTRUMENT','Channel settings','This panel will host microphone channel templates, duplication and per-instrument sample management in the production version.'));
$('#orphanedButton').addEventListener('click',()=>showInfo('SAMPLE MANAGEMENT','8 orphaned samples','These samples remain safe when velocity layers are removed. They can be auditioned and reassigned to a new layer.'));
$('#editMidiMap').addEventListener('click',()=>showInfo('MIDI MAP','Editable mapping','Click an instrument MIDI note in the editor or use MIDI Learn. Conflicting notes will be highlighted before assignment.'));
$('#exportMidi').addEventListener('click',()=>toast('MIDI map exported','swarm-midi-map.json'));
$('#collapseSide').addEventListener('click',()=>{const side=$('#sidePanel');side.classList.toggle('hidden');toast('Global panel hidden','Use Global Settings to reopen');});
$$('.tab').forEach(tab=>tab.addEventListener('click',()=>{ $$('.tab').forEach(t=>t.classList.toggle('active',t===tab)); if(tab.dataset.view==='global'){ $('#sidePanel').classList.remove('hidden');$('#sidePanel').scrollIntoView({behavior:'smooth'}); } else $('#instrumentEditor').scrollIntoView({behavior:'smooth'}); }));
document.addEventListener('keydown',e=>{if((e.metaKey||e.ctrlKey)&&e.key.toLowerCase()==='k'){e.preventDefault();refs.search.focus()}if(e.code==='Space'&&!/INPUT|SELECT|TEXTAREA|BUTTON/.test(document.activeElement.tagName)){e.preventDefault();playPreview('Keyboard preview')}});

noteOptions(); renderCards(); renderEditor(); renderThresholds();
