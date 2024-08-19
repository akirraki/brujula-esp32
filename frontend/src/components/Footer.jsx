import React from 'react';
import './Footer.css';

function Footer({ onFullClean }) {
    const handleFullClean = async () => {
        if (window.confirm('Are you sure you want to delete all files? This action cannot be undone.')) {
            try {
                const response = await fetch('/api/full-clean', {
                    method: 'DELETE',
                });
                if (!response.ok) {
                    throw new Error('Failed to delete all files');
                }
                const result = await response.text();
                alert(result);
                onFullClean(); // Call this to refetch the file list
            } catch (error) {
                console.error('Error deleting all files:', error);
                alert('Error deleting all files');
            }
        }
    };

    return (
        <footer className="footer">
            <button className="btn btn-full-clean" onClick={handleFullClean}>
                Eliminar todos los archivos
            </button>
        </footer>
    );
}

export default Footer;