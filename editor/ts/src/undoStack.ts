// Undo/redo for editor mutations. Commands are applied immediately and can be
// reverted, so scene edits stay reversible without snapshotting the whole scene.

export interface Command {
  readonly label: string;
  apply(): void;
  revert(): void;
}

const MAX_DEPTH = 256;

export class UndoStack {
  private readonly undoCommands: Command[] = [];
  private readonly redoCommands: Command[] = [];
  private readonly depth: number;

  constructor(depth: number = MAX_DEPTH) {
    if (!Number.isInteger(depth) || depth < 1) {
      throw new RangeError("undo depth must be a positive integer");
    }
    this.depth = depth;
  }

  get canUndo(): boolean {
    return this.undoCommands.length > 0;
  }

  get canRedo(): boolean {
    return this.redoCommands.length > 0;
  }

  get undoLabel(): string | null {
    return this.undoCommands.at(-1)?.label ?? null;
  }

  get redoLabel(): string | null {
    return this.redoCommands.at(-1)?.label ?? null;
  }

  get size(): number {
    return this.undoCommands.length;
  }

  execute(command: Command): void {
    command.apply();
    this.undoCommands.push(command);
    if (this.undoCommands.length > this.depth) {
      this.undoCommands.shift();
    }
    this.redoCommands.length = 0;
  }

  undo(): string | null {
    const command = this.undoCommands.pop();
    if (command === undefined) {
      return null;
    }
    command.revert();
    this.redoCommands.push(command);
    return command.label;
  }

  redo(): string | null {
    const command = this.redoCommands.pop();
    if (command === undefined) {
      return null;
    }
    command.apply();
    this.undoCommands.push(command);
    return command.label;
  }

  clear(): void {
    this.undoCommands.length = 0;
    this.redoCommands.length = 0;
  }
}
