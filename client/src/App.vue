<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref, shallowRef, watch } from "vue";
import { EditorView, basicSetup } from 'codemirror';
import { keymap } from '@codemirror/view';
import { indentSelection, indentWithTab } from '@codemirror/commands';
import { Compartment, EditorState } from '@codemirror/state';
import { cpp } from '@codemirror/lang-cpp';
import { basicLight } from '@fsegurai/codemirror-theme-basic-light';
import { basicDark } from '@fsegurai/codemirror-theme-basic-dark';
import { instance } from "@viz-js/viz";
import { linter, setDiagnostics, type Diagnostic } from "@codemirror/lint";
import type { AnalysisError, AnalysisRequest, AnalysisResponse } from "./types";
import { useDark, useStorage } from "@vueuse/core";
import { indentUnit } from "@codemirror/language";

const API_URL = import.meta.env.VITE_API_URL || 'http://localhost:8000/api';

const editorParent = ref<HTMLElement | null>(null);
const graphParent = ref<HTMLElement | null>(null);
const editorView = shallowRef<EditorView | null>(null);
const errDialog = ref<HTMLDialogElement | null>(null);

const isAnalyzing = ref(false);
const analysisError = ref<AnalysisError>();
const analysisResult = ref<AnalysisResponse>();

const themeCompartment = new Compartment();
const isDark = useDark();

const funcName = useStorage("analyzer-func-name", "test_loop");
const code = useStorage("analyzer-source-code", `void test_loop() {
    int* p = nullptr;
    for (int i = 0; i < 10; i++) {
        if (i > 5) {
            p = new int;
        }
        *p = i; // potential null dereference
    }
}`);

let viz: any = null;

onMounted(async () => {
    try {
        viz = await instance();
    } catch (e) {
        console.error("Failed to load Viz.js", e);
    }

    if (editorParent.value) {
        editorView.value = new EditorView({
            parent: editorParent.value,
            state: EditorState.create({
                doc: code.value,
                extensions: [
                    basicSetup,
                    cpp(),
                    themeCompartment.of(isDark.value ? basicDark : basicLight),
                    linter(() => []),
                    EditorState.tabSize.of(4),
                    indentUnit.of("    "),
                    keymap.of([indentWithTab]),
                    EditorView.updateListener.of((update) => {
                        if (update.docChanged) {
                            code.value = update.state.doc.toString();
                        }
                    }),
                ],
            }),
        });
    }
});

watch(isDark, (dark) => {
    if (editorView.value) {
        editorView.value.dispatch({
            effects: themeCompartment.reconfigure(dark ? basicDark : basicLight),
        });
    }
});

onBeforeUnmount(() => {
    editorView.value?.destroy();
});

async function runAnalysis() {
    if (!editorView.value || !viz) return;
    isAnalyzing.value = true;
    analysisError.value = undefined;
    analysisResult.value = undefined;
    editorView.value.dispatch(setDiagnostics(editorView.value.state, []));
    const currentCode = editorView.value.state.doc.toString();
    const req: AnalysisRequest = {
        code: currentCode,
        func: funcName.value,
    };

    try {
        const res = await fetch(`${API_URL}/analyze`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(req),
        });
        if (res.ok) {
            const data: AnalysisResponse = await res.json();
            analysisResult.value = data;
        } else {
            const errData: { detail: AnalysisError } = await res.json();
            analysisError.value = errData.detail;
            errDialog.value?.showModal();
        }
    } catch (err: any) {
        analysisError.value = { error: 'network error', stderr: err.message || String(err) };
        errDialog.value?.showModal();
    } finally {
        isAnalyzing.value = false;
    }
}

function formatCode() {
    if (!editorView.value) return;
    const view = editorView.value;
    const cursor = view.state.selection.main.head;
    view.dispatch({
        selection: { anchor: 0, head: view.state.doc.length }
    });
    indentSelection(view);
    view.dispatch({
        selection: { anchor: cursor, head: cursor }
    });
}

watch(analysisResult, (data) => {
    if (!data) return;
    if (data.issues) {
        const diagnostics: Diagnostic[] = data.issues.map((issue: any) => ({
            from: issue.start_offset,
            to: issue.end_offset,
            severity: 'error',
            message: issue.message
        }));

        if (editorView.value) {
            editorView.value.dispatch(setDiagnostics(editorView.value.state, diagnostics));
        }
    }
});

watch(graphParent, (el) => {
    if (el && analysisResult.value && analysisResult.value.dot && viz) {
        const svgElement = viz.renderSVGElement(analysisResult.value.dot);
        if (graphParent.value) {
            graphParent.value.innerHTML = '';
            graphParent.value.appendChild(svgElement);
        }
    }
});

function onClear() {
    analysisResult.value = undefined;
    analysisError.value = undefined;
    editorView.value?.dispatch(setDiagnostics(editorView.value.state, []));
}
</script>

<template>
    <div class="toolbar">
        <input id="func-name" type="text" v-model="funcName" placeholder="function name" />
        <button type="submit" @click="runAnalysis" :disabled="isAnalyzing">
            <span v-if="isAnalyzing">analyzing...</span>
            <span v-else>analyze</span>
        </button>
        <button @click="formatCode" :disabled="isAnalyzing">
            <span class="material-symbols-outlined">
                code_blocks
            </span>
        </button>
        <button @click="onClear" v-if="analysisResult">
            <span class="material-symbols-outlined">
                clear_all
            </span>
        </button>
    </div>

    <div class="layout">
        <div ref="editorParent" class="cm-container"></div>
        <template v-if="analysisResult">
            <span class="sep"></span>
            <div ref="graphParent" class="svg-container">
            </div>
        </template>
    </div>

    <dialog ref="errDialog">
        <header>
            <h3>{{ analysisError?.error }}</h3>
        </header>
        <main>
            <pre>{{ analysisError?.stderr }}</pre>
        </main>
        <footer>
            <button @click="errDialog?.close()">OK</button>
        </footer>
    </dialog>
</template>

<style scoped>
.toolbar {
    display: flex;
    gap: 0.3rem;
    border-bottom: 0.1rem solid var(--fg-color);
}

.layout {
    display: flex;
    flex-direction: column;
    flex-grow: 1;
    overflow: hidden;

    .sep {
        background-color: var(--fg-color);
        height: 0.1rem;
        width: auto;
        z-index: 3;
    }

    @media (min-width: 1024px) {
        flex-direction: row;

        .sep {
            width: 0.1rem;
            height: auto;
        }
    }
}

.cm-container {
    display: flex;
    flex: 1;
    overflow: hidden;
}

.svg-container {
    flex: 1;
    overflow: auto;

    svg {
        margin: auto 0;
        height: auto;
    }
}

/* CodeMirror Specific Overrides */
:deep(.cm-editor) {
    flex-grow: 1;
    max-width: 100%;
}

:deep(.cm-scroller) {
    font-family: 'Fira Code', monospace;
}
</style>