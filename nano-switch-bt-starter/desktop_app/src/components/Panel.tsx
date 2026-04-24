import type { ReactNode } from "react";

type PanelProps = {
  title: string;
  copy: string;
  actions?: ReactNode;
  children: ReactNode;
};

export function Panel({ title, copy, actions, children }: PanelProps) {
  return (
    <section className="panel">
      <header className="panel-header">
        <div>
          <h2 className="panel-title">{title}</h2>
          <p className="panel-copy">{copy}</p>
        </div>
        {actions}
      </header>
      {children}
    </section>
  );
}
