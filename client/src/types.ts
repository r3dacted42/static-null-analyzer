export type AnalysisRequest = {
    code: string;
    func: string;
};

export type AnalysisResponse = {
    dot: string;
    issues: {
        start_offset: number;
        end_offset: number;
        message: string;
    }[];
};

export type AnalysisError = {
    error: string;
    stderr: string;
};
