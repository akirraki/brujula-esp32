import React, { useEffect } from 'react';
import useFetch from './useFetch';
import './FilesTable.css'; // We'll create this CSS file

function FilesTable({ onRefetch }) {
    const { data: files, loading, error, refetch } = useFetch('/api/file-list');

    useEffect(() => {
        window.filesTableRefetch = refetch;
        return () => {
            delete window.filesTableRefetch;
        };
    }, [refetch]);

    const handleDownload = (filename) => {
        window.location.href = `/download/${filename}`;
    };

    const handleDelete = async (filename) => {
        if (window.confirm(`Are you sure you want to delete ${filename}?`)) {
            try {
                const response = await fetch(`/api/delete/${filename}`, {
                    method: 'DELETE',
                });
                if (!response.ok) {
                    throw new Error('Failed to delete file');
                }
                alert('File deleted successfully');
                refetch();
            } catch (error) {
                console.error('Error deleting file:', error);
                alert('Error deleting file');
            }
        }
    };

    if (loading) return <div className="files-table-message">Loading...</div>;
    if (error) return <div className="files-table-message error">Error: {error.message}</div>;

    return (
        <div className="files-table-container">
            <h2>Archivos disponibles</h2>
            <table className="files-table">
                <thead>
                    <tr>
                        <th>Nombre</th>
                        <th>Descargar</th>
                        <th>Eliminar</th>
                    </tr>
                </thead>
                <tbody>
                    {files.map(file => (
                        <tr key={file}>
                            <td>{file}</td>
                            <td>
                                <button className="btn btn-download" onClick={() => handleDownload(file)}>Descargar</button>
                            </td>
                            <td>
                                <button className="btn btn-delete" onClick={() => handleDelete(file)}>Eliminar</button>
                            </td>
                        </tr>
                    ))}
                </tbody>
            </table>
        </div>
    );
}

export default FilesTable;